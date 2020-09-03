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
using namespace CloudTools::IO;

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
	_progressMessage = "Creating CHM (" + _prefix + ")";
	newResult("CHM");
	{
		Difference<float> comparison({_dtmInputPath, _dsmInputPath}, result("CHM").path(), _progress);
		comparison.execute();
		result("CHM").dataset = comparison.target();
		_targetMetadata = comparison.targetMetadata();
	}

	_progressMessage = "Matrix transformation (" + _prefix + ")";
	newResult("antialias");
	{
		MatrixTransformation filter(result("CHM").dataset, result("antialias").path(), 1, _progress);
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
		result("antialias").dataset = filter.target();
	}
	deleteResult("CHM");

	_progressMessage = "Small points elimination (" + _prefix + ")";
	newResult("nosmall");
	{
		EliminateNonTrees elimination({result("antialias").dataset}, result("nosmall").path(), _progress);
		elimination.execute();
		result("nosmall").dataset = elimination.target();
	}
	deleteResult("antialias");

	newResult("interpol");
	{
		_progressMessage = "Interpolation (" + _prefix + ")";
		InterpolateNoData interpolation({result("nosmall").dataset}, result("interpol").path(), _progress);
		interpolation.execute();
		result("interpol").dataset = interpolation.target();
	}

	_progressMessage = "Seed points collection (" + _prefix + ")";
	std::vector<OGRPoint> seedPoints = collectSeedPoints(result("interpol").dataset);

	_progressMessage = "Tree crown segmentation (" + _prefix + ")";
	{
		TreeCrownSegmentation segmentation(result("interpol").dataset, seedPoints, _progress);
		segmentation.execute();
		_targetCluster = segmentation.clusterMap();
	}
	writeClusterMapToFile((fs::path(_outputDir) / (_prefix + "_segmentation.tif")).string());
	deleteResult("interpol");

	for (int i = 0; i < morphologyCounter; ++i)
	{
		_progressMessage = "Morphological erosion "
		                   + std::to_string(i + 1) + "/" + std::to_string(morphologyCounter)
		                   + " (" + _prefix + ")";
		MorphologyClusterFilter erosion(_targetCluster, {result("nosmall").dataset}, nullptr,
		                                MorphologyClusterFilter::Method::Erosion, _progress);
		erosion.threshold = erosionThreshold;
		erosion.execute();

		_progressMessage = "Morphological dilation "
		                   + std::to_string(i + 1) + "/" + std::to_string(morphologyCounter)
		                   + " (" + _prefix + ")";
		MorphologyClusterFilter dilation(erosion.target(), {result("nosmall").dataset}, nullptr,
		                                 MorphologyClusterFilter::Method::Dilation, _progress);
		dilation.execute();

		_targetCluster = dilation.target();
	}

	_targetCluster.removeSmallClusters(removalRadius);
	writeClusterMapToFile((fs::path(_outputDir) / (_prefix + "_morphology.tif")).string());
	deleteResult("nosmall");
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

Result* PreProcess::createResult(const std::string& name, bool isFinal)
{
	std::string filename = _prefix + "_" + name + ".tif";

	if (isFinal || debug)
		return new PermanentFileResult(fs::path(_outputDir) / filename);
	else
		return new TemporaryFileResult(fs::path(_outputDir) / filename);
}
} // Vegetation
} // AHN
