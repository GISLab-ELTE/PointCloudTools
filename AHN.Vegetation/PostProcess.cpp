#include <numeric>

#include <gdal_priv.h>

#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>

#include "PostProcess.h"
#include "HausdorffDistance.h"
#include "CentroidDistance.h"
#include "VolumeDifference.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
void PostProcess::writeClusterPairsToFile(const std::string& outPath, std::shared_ptr<DistanceCalculation> distance)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(outPath) &&
	    driver->Delete(outPath.c_str()) == CE_Failure &&
	    !fs::remove(outPath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* target = driver->Create(outPath.c_str(),
	                                     _rasterMetadata.rasterSizeX(),
	                                     _rasterMetadata.rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&_rasterMetadata.geoTransform()[0]);
	if (_rasterMetadata.reference().Validate() == OGRERR_NONE)
	{
		char *wkt;
		_rasterMetadata.reference().exportToWkt(&wkt);
		target->SetProjection(wkt);
		CPLFree(wkt);
	}

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	std::srand(42); // Fixed seed, so the random shuffling is reproducible.
	int commonId;
	std::vector<OGRPoint> points;

	int numberOfClusters = distance->closest().size();
	std::vector<int> ids(numberOfClusters);
	std::iota(ids.begin(), ids.end(), 0);
	std::random_shuffle(ids.begin(), ids.end());

	for (auto elem : distance->closest())
	{
		commonId = ids.back();
		ids.pop_back();

		points = _clustersAHN2.points(elem.first.first);
		CPLErr ioResult = CE_None;
		for (const auto& point : points)
		{
			ioResult = static_cast<CPLErr>(ioResult |
			                               targetBand->RasterIO(GF_Write,
			                                                    point.getX(), point.getY(),
			                                                    1, 1,
			                                                    &commonId,
			                                                    1, 1,
			                                                    gdalType<int>(),
			                                                    0, 0));
		}

		points = _clustersAHN3.points(elem.first.second);
		for (const auto& point : points)
		{
			ioResult = static_cast<CPLErr>(ioResult |
			                               targetBand->RasterIO(GF_Write,
			                                                    point.getX(), point.getY(),
			                                                    1, 1,
			                                                    &commonId,
			                                                    1, 1,
			                                                    gdalType<int>(),
			                                                    0, 0));
		}

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occured.");
	}

	commonId = -2;
	for (auto elem : distance->lonelyAHN2())
	{
		points = _clustersAHN2.points(elem);
		CPLErr ioResult = CE_None;
		for (const auto& point : points)
		{
			ioResult = static_cast<CPLErr>(ioResult |
			                               targetBand->RasterIO(GF_Write,
			                                                    point.getX(), point.getY(),
			                                                    1, 1,
			                                                    &commonId,
			                                                    1, 1,
			                                                    gdalType<int>(),
			                                                    0, 0));
		}

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occurred.");
	}

	commonId = -3;
	for (auto elem : distance->lonelyAHN3())
	{
		points = _clustersAHN3.points(elem);
		CPLErr ioResult = CE_None;
		for (const auto& point : points)
		{
			ioResult = static_cast<CPLErr>(ioResult |
			                               targetBand->RasterIO(GF_Write,
			                                                    point.getX(), point.getY(),
			                                                    1, 1,
			                                                    &commonId,
			                                                    1, 1,
			                                                    gdalType<int>(),
			                                                    0, 0));
		}

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occurred.");
	}

	GDALClose(target);
}

void PostProcess::writeClusterHeightsToFile(const std::string& outPath, std::shared_ptr<DistanceCalculation> distance)
{
	std::map<std::pair<int, int>, float> heightMap;
	for (const auto& elem : distance->closest())
	{
		float ahn2ClusterHeight = std::accumulate(
			_clustersAHN2.points(elem.first.first).begin(),
			_clustersAHN2.points(elem.first.first).end(), 0.0,
			[](float sum, const OGRPoint& point)
			{
				return sum + point.getZ();
			});

		float ahn3ClusterHeight = std::accumulate(
			_clustersAHN3.points(elem.first.second).begin(),
			_clustersAHN3.points(elem.first.second).end(), 0.0,
			[](float sum, const OGRPoint& point)
			{
				return sum + point.getZ();
			});

		float heightDiff = ahn3ClusterHeight - ahn2ClusterHeight;
		float avgHeightDiff = heightDiff /
		                      std::max(_clustersAHN2.points(elem.first.first).size(),
		                               _clustersAHN3.points(elem.first.second).size());

		for (const OGRPoint& point : _clustersAHN2.points(elem.first.first))
		{
			heightMap[std::make_pair(point.getX(), point.getY())] = avgHeightDiff;
		}

		for (const OGRPoint& point : _clustersAHN3.points(elem.first.second))
		{
			heightMap[std::make_pair(point.getX(), point.getY())] = avgHeightDiff;
		}
	}

	SweepLineTransformation<float> heightWriter(
		{_ahn2DSMInputPath, _ahn3DSMInputPath}, outPath, 0, nullptr, _progress);

	heightWriter.computation = [&heightWriter, &heightMap](int x, int y,
	                                                        const std::vector<Window<float>>& sources)
	{
		const Window<float>& ahn2 = sources[0];
		const Window<float>& ahn3 = sources[1];

		if (!ahn2.hasData() || !ahn3.hasData())
			return static_cast<float>(heightWriter.nodataValue);

		auto index = std::make_pair(x, y);
		if (!heightMap.count(index))
			return static_cast<float>(heightWriter.nodataValue);
		else
			return heightMap[index];
	};

	heightWriter.execute();
}

void PostProcess::onPrepare()
{
	if (_ahn2DSMInputPath.empty() || _ahn3DSMInputPath.empty())
		throw std::runtime_error("Defining the surface DEM files is mandatory.");

	// Read raster metadata
	GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(_ahn2DSMInputPath.c_str(), GA_ReadOnly));
	_rasterMetadata = RasterMetadata(dataset);
	GDALClose(dataset);

	// Piping the internal progress reporter to override message.
	if (progress)
		_progress = [this](float complete, std::string message)
		{
			return this->progress(complete, this->_progressMessage);
		};
	else
		_progress = nullptr;
}

void PostProcess::onExecute()
{
	std::shared_ptr<DistanceCalculation> distance;
	if (_method == Hausdorff)
	{
		_progressMessage = "Hausdorff distance calculation to pair up clusters";
		distance.reset(new HausdorffDistance(_clustersAHN2, _clustersAHN3));
	}

	if (_method == Centroid)
	{
		_progressMessage = "Centroid (gravity) distance calculation to pair up clusters";
		distance.reset(new CentroidDistance(_clustersAHN2, _clustersAHN3));
	}

	distance->progress = _progress;
	distance->execute();
	writeClusterPairsToFile((fs::path(_outputDir) / "cluster_pairs.tif").string(), distance);

	std::cout << std::endl;
	std::cout << "Total number of clusters in AHN2: " << _clustersAHN2.clusterIndexes().size() << std::endl;
	std::cout << "Total number of clusters in AHN3: " << _clustersAHN3.clusterIndexes().size() << std::endl;
	std::cout << "Pairs found: " << distance->closest().size() << std::endl;
	std::cout << "Number of unpaired clusters in AHN2: " << distance->lonelyAHN2().size() << std::endl;
	std::cout << "Number of unpaired clusters in AHN3: " << distance->lonelyAHN3().size() << std::endl;

	VolumeDifference volumeDifference(_clustersAHN2, _clustersAHN3, distance);

	std::cout << "AHN2 full volume: " << volumeDifference.ahn2_fullVolume << std::endl;
	std::cout << "AHN3 full volume: " << volumeDifference.ahn3_fullVolume << std::endl;
	std::cout << "AHN2 and AHN3 difference: " << (volumeDifference.ahn3_fullVolume - volumeDifference.ahn2_fullVolume)
	          << std::endl;

	_progressMessage = "Height map";
	writeClusterHeightsToFile((fs::path(_outputDir) / "cluster_heights.tif").string(), distance);
}
} // Vegetation
} // AHN