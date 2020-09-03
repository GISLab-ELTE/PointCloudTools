#include <numeric>

#include <gdal_priv.h>

#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>

#include "PreProcess.h"
#include "EliminateNonTrees.h"
#include "InterpolateNoData.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
void PreProcess::onPrepare()
{
	if (_dtmInputPath.empty() || _dsmInputPath.empty())
		throw std::runtime_error("Defining both the terrain and the surface DEM files is mandatory.");

	// Piping the internal progress reporter to override message.
	if (progress)
		_progress = [this](float complete, std::string message)
		{
			return this->progress(complete, this->_progressMessage);
		};
	else
		_progress = nullptr;
}

void PreProcess::onExecute()
{
	std::string chmOut, antialiasOut, nosmallOut, interpolOut, segmentationOut, morphologyOut;

	chmOut = (fs::path(_outputDir) / (_prefix + "_CHM.tif")).string();
	antialiasOut = (fs::path(_outputDir) / (_prefix + "_antialias.tif")).string();
	nosmallOut = (fs::path(_outputDir) / (_prefix + "_nosmall.tif")).string();
	interpolOut = (fs::path(_outputDir) / (_prefix + "_interpol.tif")).string();
	segmentationOut = (fs::path(_outputDir) / (_prefix + "_segmentation.tif")).string();
	morphologyOut = (fs::path(_outputDir) / (_prefix + "_morphology.tif")).string();


	_progressMessage = "Creating CHM (" + _prefix + ")";
	Difference<float> comparison({_dtmInputPath, _dsmInputPath}, chmOut, _progress);
	comparison.execute();

	_targetMetadata = comparison.targetMetadata();

	_progressMessage = "Matrix transformation (" + _prefix + ")";
	MatrixTransformation filter(comparison.target(), antialiasOut, 1, _progress);
	filter.setMatrix(0, 0, 4); // middle
	filter.setMatrix(0, -1, 2); // sides
	filter.setMatrix(0, 1, 2);
	filter.setMatrix(-1, 0, 2);
	filter.setMatrix(1, 0, 2);
	filter.setMatrix(-1, -1, 1); // corners
	filter.setMatrix(1, -1, 1);
	filter.setMatrix(-1, 1, 1);
	filter.setMatrix(1, 1, 1);
	filter.execute();

	_progressMessage = "Small points elimination (" + _prefix + ")";
	EliminateNonTrees elimination({filter.target()}, nosmallOut, _progress);
	elimination.execute();

	_progressMessage = "Interpolation (" + _prefix + ")";
	InterpolateNoData interpolation({elimination.target()}, interpolOut, _progress);
	interpolation.execute();

	_progressMessage = "Seed points collection (" + _prefix + ")";
	std::vector<OGRPoint> seedPoints = collectSeedPoints(interpolation.target());

	_progressMessage = "Tree crown segmentation (" + _prefix + ")";
	TreeCrownSegmentation segmentation({interpolation.target()}, seedPoints, _progress);
	segmentation.execute();

	_targetCluster = segmentation.clusterMap();
	writeClusterMapToFile(segmentationOut);

	for (int i = 0; i < morphologyCounter; ++i)
	{
		_progressMessage = "Morphological erosion "
		                   + std::to_string(i + 1) + "/" + std::to_string(morphologyCounter)
		                   + " (" + _prefix + ")";
		MorphologyClusterFilter erosion(_targetCluster, {elimination.target()}, nullptr,
		                                MorphologyClusterFilter::Method::Erosion, _progress);
		erosion.threshold = erosionThreshold;
		erosion.execute();

		_progressMessage = "Morphological dilation "
		                   + std::to_string(i + 1) + "/" + std::to_string(morphologyCounter)
		                   + " (" + _prefix + ")";
		MorphologyClusterFilter dilation(erosion.target(), {elimination.target()}, nullptr,
		                                 MorphologyClusterFilter::Method::Dilation, _progress);
		dilation.execute();

		_targetCluster = dilation.target();
	}

	_targetCluster.removeSmallClusters(removalRadius);
	writeClusterMapToFile(morphologyOut);
}

std::vector<OGRPoint> PreProcess::collectSeedPoints(GDALDataset* target)
{
	SweepLineCalculation<float> collectSeeds({target}, 7, nullptr, _progress);

	std::vector<OGRPoint> seedPoints;
	collectSeeds.computation = [&collectSeeds, &seedPoints](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData())
			return;

		for (int i = -collectSeeds.range(); i <= collectSeeds.range(); i++)
			for (int j = -collectSeeds.range(); j <= collectSeeds.range(); j++)
				if (source.data(i, j) > source.data(0, 0))
					return;
		seedPoints.emplace_back(x, y, source.data(0, 0));
	};

	collectSeeds.execute();
	return seedPoints;
}

void PreProcess::writeClusterMapToFile(const std::string& outPath)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(outPath) &&
	    driver->Delete(outPath.c_str()) == CE_Failure &&
	    !fs::remove(outPath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* target = driver->Create(outPath.c_str(),
	                                     _targetMetadata.rasterSizeX(),
	                                     _targetMetadata.rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&_targetMetadata.geoTransform()[0]);
	if (_targetMetadata.reference().Validate() == OGRERR_NONE)
	{
		char *wkt;
		_targetMetadata.reference().exportToWkt(&wkt);
		target->SetProjection(wkt);
		CPLFree(wkt);
	}

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	srand(time(NULL));
	int commonId;
	std::vector<OGRPoint> points;

	int numberOfClusters = _targetCluster.clusterIndexes().size();
	std::vector<int> ids(numberOfClusters);
	std::iota(ids.begin(), ids.end(), 0);
	std::random_shuffle(ids.begin(), ids.end());

	for (GUInt32 index : _targetCluster.clusterIndexes())
	{
		commonId = ids.back();
		ids.pop_back();

		points = _targetCluster.points(index);
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
			throw std::runtime_error("Target write error occured.");
	}

	GDALClose(target);
}
} // Vegetation
} // AHN
