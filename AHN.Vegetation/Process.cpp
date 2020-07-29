#include "Process.h"

#include <numeric>

#include <CloudTools.DEM/SweepLineCalculation.hpp>

#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>
#include <future>
#include "EliminateNonTrees.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"
#include "HausdorffDistance.h"
#include "CentroidDistance.h"
#include "VolumeDifference.h"
#include "HeightDifference.h"

using namespace CloudTools::IO;
using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
bool Process::runReporter(CloudTools::DEM::Calculation* operation)
{
	operation->progress = [this](float complete, const std::string& message)
	{
		this->reporter->report(complete, message);
		return true;
	};
	return false;
}

std::vector<OGRPoint> Process::collectSeedPoints(GDALDataset* target, po::variables_map& vm)
{
	SweepLineCalculation<float> collectSeeds({ target }, 7, nullptr);

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

	if (!vm.count("quiet"))
	{
		collectSeeds.progress = [this](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	collectSeeds.execute();
	return seedPoints;
}

void Process::writeClusterMapToFile(const ClusterMap& cluster,
	const RasterMetadata& metadata, const std::string& outpath)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(outpath) &&
		driver->Delete(outpath.c_str()) == CE_Failure &&
		!fs::remove(outpath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* target = driver->Create(outpath.c_str(),
		metadata.rasterSizeX(),
		metadata.rasterSizeY(), 1,
		gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&metadata.geoTransform()[0]);

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	srand(time(NULL));
	int commonId;
	std::vector<OGRPoint> points;

	int numberOfClusters = cluster.clusterIndexes().size();
	std::vector<int> ids(numberOfClusters);
	std::iota(ids.begin(), ids.end(), 0);
	std::random_shuffle(ids.begin(), ids.end());

	for (GUInt32 index : cluster.clusterIndexes())
	{
		commonId = ids.back();
		ids.pop_back();

		points = cluster.points(index);
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

void Process::writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
	const std::string& ahn2Outpath, std::shared_ptr<DistanceCalculation> distance)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(ahn2Outpath) &&
		driver->Delete(ahn2Outpath.c_str()) == CE_Failure &&
		!fs::remove(ahn2Outpath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* target = driver->Create(ahn2Outpath.c_str(),
		targetMetadata.rasterSizeX(),
		targetMetadata.rasterSizeY(), 1,
		gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&targetMetadata.geoTransform()[0]);

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	srand(time(NULL));
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

		points = ahn2Map.points(elem.first.first);
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

		points = ahn3Map.points(elem.first.second);
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
		points = ahn2Map.points(elem);
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

	commonId = -3;
	for (auto elem : distance->lonelyAHN3())
	{
		points = ahn3Map.points(elem);
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

void Process::writeClusterHeightsToFile(
	ClusterMap& ahn2Map, ClusterMap& ahn3Map,
	const std::string& ahn2DSM, const std::string& ahn3DSM,
	const std::string& outpath, std::shared_ptr<DistanceCalculation> distance,
	CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	std::map<std::pair<int, int>, float> heightMap;
	for (const auto& elem : distance->closest())
	{
		float ahn2ClusterHeight = std::accumulate(ahn2Map.points(elem.first.first).begin(),
																							ahn2Map.points(elem.first.first).end(), 0.0,
																							[](float sum, const OGRPoint& point)
																							{
																								return sum + point.getZ();
																							});

		float ahn3ClusterHeight = std::accumulate(ahn3Map.points(elem.first.second).begin(),
																							ahn3Map.points(elem.first.second).end(), 0.0,
																							[](float sum, const OGRPoint& point)
																							{
																								return sum + point.getZ();
																							});

		float heightDiff = ahn3ClusterHeight - ahn2ClusterHeight;
		float avgHeightDiff = heightDiff /
													std::max(ahn2Map.points(elem.first.first).size(), ahn3Map.points(elem.first.second).size());

		for(const OGRPoint& point : ahn2Map.points(elem.first.first))
		{
			heightMap[std::make_pair(point.getX(), point.getY())] = avgHeightDiff;
		}

		for (const OGRPoint& point : ahn3Map.points(elem.first.second))
		{
			heightMap[std::make_pair(point.getX(), point.getY())] = avgHeightDiff;
		}
	}

	SweepLineTransformation<float>* heightWriter = new SweepLineTransformation<float>(
		{ ahn2DSM, ahn3DSM }, outpath, 0, nullptr);

	heightWriter->computation = [&heightWriter, &heightMap](int x, int y,
																													const std::vector<Window<float>>& sources)
	{
		const Window<float>& ahn2 = sources[0];
		const Window<float>& ahn3 = sources[1];

		if (!ahn2.hasData() || !ahn3.hasData())
			return static_cast<float>(heightWriter->nodataValue);

		auto index = std::make_pair(x, y);
		if (!heightMap.count(index))
			return static_cast<float>(heightWriter->nodataValue);
		else
			return heightMap[index];
	};

	if (!vm.count("quiet"))
	{
		heightWriter->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	heightWriter->execute();

	std::cout << "Height map written." << std::endl;
}

ClusterMap Process::preprocess(int version)
{
	std::string chmOut, antialiasOut, nosmallOut, interpolOut, segmentationOut, morphologyOut;
	std::string dtm, dsm;
	ClusterMap cluster;

	if (version == 3)
	{
		chmOut = (fs::path(outputDir) / "ahn3_CHM.tif").string();
		antialiasOut = (fs::path(outputDir) / "ahn3_antialias.tif").string();
		nosmallOut = (fs::path(outputDir) / "ahn3_nosmall.tif").string();
		interpolOut = (fs::path(outputDir) / "ahn3_interpol.tif").string();
		segmentationOut = (fs::path(outputDir) / "ahn3_segmentation.tif").string();
		morphologyOut = (fs::path(outputDir) / "ahn3_morphology.tif").string();
		dsm = AHN3DSMInputPath;
		dtm = AHN3DTMInputPath;
	}

	if (version == 2)
	{
		chmOut = (fs::path(outputDir) / "ahn2_CHM.tif").string();
		antialiasOut = (fs::path(outputDir) / "ahn2_antialias.tif").string();
		nosmallOut = (fs::path(outputDir) / "ahn2_nosmall.tif").string();
		interpolOut = (fs::path(outputDir) / "ahn2_interpol.tif").string();
		segmentationOut = (fs::path(outputDir) / "ahn2_segmentation.tif").string();
		morphologyOut = (fs::path(outputDir) / "ahn2_morphology.tif").string();
		dsm = AHN2DSMInputPath;
		dtm = AHN2DTMInputPath;
	}

	Difference<float> comparison({ dtm, dsm }, chmOut);
	if (!vm.count("quiet"))
	{
		runReporter(&comparison);
	}
	comparison.execute();
	reporter->reset();
	std::cout << "CHM created." << std::endl;

	this->targetMetadata = comparison.targetMetadata();

	MatrixTransformation filter(comparison.target(), antialiasOut, 1);
	filter.setMatrix(0, 0, 4); // middle
	filter.setMatrix(0, -1, 2); // sides
	filter.setMatrix(0, 1, 2);
	filter.setMatrix(-1, 0, 2);
	filter.setMatrix(1, 0, 2);
	filter.setMatrix(-1, -1, 1); // corners
	filter.setMatrix(1, -1, 1);
	filter.setMatrix(-1, 1, 1);
	filter.setMatrix(1, 1, 1);

	if (!vm.count("quiet"))
	{
		runReporter(&filter);
	}
	filter.execute();
	reporter->reset();
	std::cout << "Matrix transformation executed.." << std::endl;

	EliminateNonTrees elimination({ filter.target() }, nosmallOut);
	if (!vm.count("quiet"))
	{
		runReporter(&elimination);
	}
	elimination.execute();
	reporter->reset();
	std::cout << "Small points eliminated.." << std::endl;

	InterpolateNoData interpolation({ elimination.target() }, interpolOut);
	if (!vm.count("quiet"))
	{
		runReporter(&interpolation);
	}
	interpolation.execute();
	reporter->reset();
	std::cout << "Interpolation is done." << std::endl;

	std::vector<OGRPoint> seedPoints = collectSeedPoints(interpolation.target(), vm);
	std::cout << "Seed points collected." << std::endl;

	//ClusterMap cluster;
	cluster.setSizeX(this->targetMetadata.rasterSizeX());
	cluster.setSizeY(this->targetMetadata.rasterSizeY());

	TreeCrownSegmentation segmentation({ interpolation.target() }, seedPoints);
	if (!vm.count("quiet"))
	{
		runReporter(&segmentation);
	}
	segmentation.execute();
	reporter->reset();
	std::cout << "Tree crown segmentation is done." << std::endl;

	cluster = segmentation.clusterMap();

	writeClusterMapToFile(cluster, targetMetadata, segmentationOut);

	int morphologyCounter = 3, erosionThreshold = 6;
	for (int i = 0; i < morphologyCounter; ++i)
	{
		MorphologyClusterFilter erosion(cluster, { elimination.target() }, nullptr,
			MorphologyClusterFilter::Method::Erosion, nullptr);
		erosion.threshold = erosionThreshold;
		erosion.execute();
		std::cout << "Morphological erosion performed." << std::endl;

		MorphologyClusterFilter dilation(erosion.clusterMap, { elimination.target() }, nullptr,
			MorphologyClusterFilter::Method::Dilation, nullptr);
		dilation.execute();
		std::cout << "Morphological dilation performed." << std::endl;

		cluster = dilation.clusterMap;
	}

	int removalRadius = 16;
	cluster.removeSmallClusters(removalRadius);

	writeClusterMapToFile(cluster, targetMetadata, morphologyOut);

	return cluster;
}

void Process::process()
{
	std::shared_ptr<DistanceCalculation> distance;
	if (method == Hausdorff)
	{
		std::cout << "Using Hausdorff distance to pair up clusters." << std::endl;
		distance.reset(new HausdorffDistance(clusterAHN2, clusterAHN3));
		std::cout << "Hausdorff distance calculated." << std::endl;
	}

	if (method == Centroid)
	{
		std::cout << "Using centroid distance to pair up clusters." << std::endl;
		distance.reset(new CentroidDistance(clusterAHN2, clusterAHN3));
		std::cout << "Gravity distance calculated." << std::endl;
	}

	distance->execute();
	writeClusterPairsToFile(clusterAHN2, clusterAHN3, targetMetadata,
		(fs::path(outputDir) / "cluster_pairs.tif").string(), distance);

	std::cout << "Total number of clusters in AHN2: " << clusterAHN2.clusterIndexes().size() << std::
		endl;
	std::cout << "Total number of clusters in AHN3: " << clusterAHN3.clusterIndexes().size() << std::
		endl;
	std::cout << "Pairs found: " << distance->closest().size() << std::endl;
	std::cout << "Number of unpaired clusters in AHN2: " << distance->lonelyAHN2().size() << std::endl;
	std::cout << "Number of unpaired clusters in AHN3: " << distance->lonelyAHN3().size() << std::endl;

	VolumeDifference volumeDifference(clusterAHN2, clusterAHN3, distance);

	std::cout << "AHN2 full volume: " << volumeDifference.ahn2_fullVolume << std::endl;
	std::cout << "AHN3 full volume: " << volumeDifference.ahn3_fullVolume << std::endl;
	std::cout << "AHN2 and AHN3 difference: " << (volumeDifference.ahn3_fullVolume - volumeDifference.ahn2_fullVolume) << std::endl;

	writeClusterHeightsToFile(clusterAHN2, clusterAHN3, AHN2DSMInputPath, AHN3DSMInputPath,
		(fs::path(outputDir) / "cluster_heights.tif").string(), distance, reporter, vm);
}

void Process::run()
{
	std::future<ClusterMap> ahn2Future = std::async(
		vm.count("parallel") ? std::launch::async : std::launch::deferred, &Process::preprocess, this, 2);

	std::future<ClusterMap> ahn3Future = std::async(
			vm.count("parallel") ? std::launch::async : std::launch::deferred, &Process::preprocess, this, 3);

	clusterAHN2 = ahn2Future.get();
	clusterAHN3 = ahn3Future.get();

	process();
}
}
}