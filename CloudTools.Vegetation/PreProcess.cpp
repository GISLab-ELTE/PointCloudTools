#include <numeric>
#include <random>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>

#include "PreProcess.h"
#include "EliminateNonTrees.h"
#include "InterpolateNoData.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"
#include "RiverMask.hpp"
#include "BuildingFacadeSeedRemoval.hpp"

using namespace CloudTools::DEM;
using namespace CloudTools::IO;

namespace CloudTools
{
namespace Vegetation
{
void PreProcess::onPrepare()
{
	if (_dtmInputPath.empty() || _dsmInputPath.empty())
		throw std::runtime_error("Defining both the terrain and the surface DEM files is mandatory.");

	// Piping the internal progress reporter to override message.
	if (progress)
		_progress = [this](float complete, const std::string& message)
		{
			return this->progress(complete, this->_progressMessage);
		};
	else
		_progress = nullptr;
}

void PreProcess::onExecute()
{
	if(_processingMethod == PreProcess::SeedRemoval)
	{
		_progressMessage = "Creating River Map (" + _prefix + ")";
		newResult("RM");
		{
			RiverMask<float> riverMap({_dtmInputPath, _dsmInputPath}, result("RM").path(), _progress);
			riverMap.execute();
			result("RM").dataset = riverMap.target();
			_targetMetadata = riverMap.targetMetadata();
		}

		_progressMessage = "Creating CHM (" + _prefix + ")";
		newResult("CHM");
		{
			auto dsm = static_cast<GDALDataset*>(GDALOpen(_dsmInputPath.c_str(), GA_ReadOnly));
			Difference<float> comparison({{result("RM").dataset},
			                              {dsm}}, result("CHM").path(), _progress);
			comparison.execute();
			result("CHM").dataset = comparison.target();
			_targetMetadata = comparison.targetMetadata();
		}
	} else {
		_progressMessage = "Creating CHM (" + _prefix + ")";
		newResult("CHM");
		{
			Difference<float> comparison({_dtmInputPath, _dsmInputPath}, result("CHM").path(), _progress);
			comparison.execute();
			result("CHM").dataset = comparison.target();
			_targetMetadata = comparison.targetMetadata();
		}
	}

	_progressMessage = "Matrix transformation (" + _prefix + ")";
	newResult("antialias");
	{
		result("antialias").dataset = this->blur3x3Middle4(result("CHM").dataset, result("antialias").path());
		//result("antialias").dataset = this->blur3x3Middle12(result("CHM").dataset, result("antialias").path());
		//result("antialias").dataset = this->blur5x5Middle36(result("CHM").dataset, result("antialias").path());
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
	if (debug)
	{
		writePointsToFile(seedPoints, (fs::path(_outputDir) / (_prefix + "_seedpoints.json")).string());
	}

	if(_processingMethod == PreProcess::SeedRemoval)
	{
		_progressMessage = "Seed Removal(" + _prefix + ")";
		::BuildingFacadeSeedRemoval<float> seedRemoval(seedPoints, {_dtmInputPath, _dsmInputPath}, _progress);
		seedRemoval.execute();
	}

	_progressMessage = "Tree crown segmentation (" + _prefix + ")";
	{
		TreeCrownSegmentation segmentation(result("interpol").dataset, seedPoints, _progress);
		segmentation.execute();
		_targetCluster = segmentation.clusterMap();
	}
	deleteResult("interpol");
	writeClusterMapToFile((fs::path(_outputDir) / (_prefix + "_segmentation.tif")).string());

	for (std::size_t i = 0; i < morphologyCounter; ++i)
	{
		_progressMessage = "Morphological erosion "
		                   + std::to_string(i + 1) + "/" + std::to_string(morphologyCounter)
		                   + " (" + _prefix + ")";
		MorphologyClusterFilter erosion(_targetCluster, {result("nosmall").dataset},
		                                MorphologyClusterFilter::Method::Erosion, _progress);
		erosion.threshold = erosionThreshold;
		erosion.execute();

		_progressMessage = "Morphological dilation "
		                   + std::to_string(i + 1) + "/" + std::to_string(morphologyCounter)
		                   + " (" + _prefix + ")";
		MorphologyClusterFilter dilation(erosion.target(), {result("nosmall").dataset},
		                                 MorphologyClusterFilter::Method::Dilation, _progress);
		dilation.execute();

		_targetCluster = dilation.target();
	}
	deleteResult("nosmall");

	_progressMessage = "Remove small and deformed trees (" + _prefix + ")";
	if (_progress)
		_progress(0, "Removing small and deformed trees.");
	_targetCluster.removeSmallClusters(removalRadius);
	if (_progress)
		_progress(0.5, "Small clusters removed.");
	removeDeformedClusters(_targetCluster);
	if (_progress)
		_progress(1.0, "Deformed clusters removed.");
	
	writeClusterMapToFile((fs::path(_outputDir) / (_prefix + "_morphology.tif")).string());

	if (debug)
	{
		std::vector<OGRPoint> clusterPoints;
		clusterPoints.reserve(_targetCluster.clusterIndexes().size());
		for(auto index : _targetCluster.clusterIndexes())
		{
			clusterPoints.push_back(_targetCluster.center3D(index));
		}
		writePointsToFile(clusterPoints, (fs::path(_outputDir) / (_prefix + "_clusterpoints.json")).string());
	}
}

GDALDataset* PreProcess::blur3x3Middle4(GDALDataset* sourceDataset, const std::string& targetPath)
{
	MatrixTransformation filter(sourceDataset, targetPath, 1, _progress);
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
	return filter.target();
}
GDALDataset* PreProcess::blur3x3Middle12(GDALDataset* sourceDataset, const std::string& targetPath)
{
	MatrixTransformation filter(sourceDataset, targetPath, 1, _progress);
	filter.setMatrix(0, 0, 12); // middle
	filter.setMatrix(0, -1, 3); // sides
	filter.setMatrix(0, 1, 3);
	filter.setMatrix(-1, 0, 3);
	filter.setMatrix(1, 0, 3);
	filter.setMatrix(-1, -1, 1); // corners
	filter.setMatrix(1, -1, 1);
	filter.setMatrix(-1, 1, 1);
	filter.setMatrix(1, 1, 1);

	filter.execute();
	return filter.target();
}

GDALDataset* PreProcess::blur5x5Middle36(GDALDataset* sourceDataset, const std::string& targetPath)
{
	MatrixTransformation filter(sourceDataset, targetPath, 2, _progress);
	for (int i = -2; i <= 2; ++i)
	{
		for (int j = -2; j <= -2; ++j)
		{
			if (i % 2 == 0 && j % 2 == 0)
				filter.setMatrix(i, j, 1); // corners
			else if (i == 0 && j == 0)
				filter.setMatrix(i, j, 36); // middle
			else if (std::abs(i) == 1 && std::abs(j) == 2 ||
			         std::abs(i) == 2 && std::abs(j) == 1)
				filter.setMatrix(i, j, 4);
			else if (std::abs(i) == 0 && std::abs(j) == 1 ||
			         std::abs(i) == 1 && std::abs(j) == 0)
				filter.setMatrix(i, j, 24);
			else if (std::abs(i) == 2 && j == 0 ||
			         std::abs(j) == 2 && i == 0)
				filter.setMatrix(i, j, 6);
			else if (std::abs(i) == 1 && std::abs(j) == 1)
				filter.setMatrix(i, j, 16);
		}
	}

	filter.execute();
	return filter.target();
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

void PreProcess::removeDeformedClusters(ClusterMap& clusterMap)
{
	for (const GUInt32 index : clusterMap.clusterIndexes())
	{
		const auto& bbox = clusterMap.boundingBox(index);
		int minX = bbox[0].getX(), maxX = bbox[0].getX(), minY = bbox[0].getY(), maxY = bbox[0].getY();
		for (size_t i = 1; i < 4; ++i)
		{
			if (bbox[i].getX() < minX)
				minX = bbox[i].getX();
			else if (bbox[i].getX() > maxX)
				maxX = bbox[i].getX();
			if (bbox[i].getY() < minY)
				minY = bbox[i].getY();
			else if (bbox[i].getY() > maxY)
				maxY = bbox[i].getY();
		}

		int sizeX = maxX - minX;
		int sizeY = maxY - minY;

		if (sizeX < sizeY * 0.5 ||
			sizeY < sizeX * 0.5 ||
			clusterMap.points(index).size() < sizeX * sizeY * 0.5)
		{
			// Cluster is deformed, so remove.
			clusterMap.removeCluster(index);
		}
	}
}

void PreProcess::writePointsToFile(std::vector<OGRPoint> points, const std::string& outPath)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(outPath) &&
	    driver->Delete(outPath.c_str()) == CE_Failure &&
	    !fs::remove(outPath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* ds = driver->Create(outPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
	if (ds == nullptr)
		throw std::runtime_error("Target file creation failed.");

	OGRLayer* layer = ds->CreateLayer("points", &_targetMetadata.reference(), wkbPoint, nullptr);
	OGRFeature *feature;
	for(auto& point : points)
	{
		point.setX(_targetMetadata.originX() + point.getX() * _targetMetadata.pixelSizeX());
		point.setY(_targetMetadata.originY() + point.getY() * _targetMetadata.pixelSizeY());
		feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
		feature->SetGeometry(&point);
		if (layer->CreateFeature(feature) != OGRERR_NONE)
			throw std::runtime_error("Feature creation failed.");
		OGRFeature::DestroyFeature(feature);
	}

	GDALClose(ds);
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

	int commonId;
	std::vector<OGRPoint> points;

	int numberOfClusters = _targetCluster.clusterIndexes().size();
	std::vector<int> ids(numberOfClusters);
	std::iota(ids.begin(), ids.end(), 0);
	std::shuffle(ids.begin(), ids.end(), std::default_random_engine(42));
	// Fixed seed, so the random shuffling is reproducible.

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
} // CloudTools
