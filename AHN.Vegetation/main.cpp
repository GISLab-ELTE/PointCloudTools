#include <iostream>
#include <string>
#include <utility>
#include <algorithm>
#include <numeric>
#include <future>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/Filters/MorphologyFilter.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>

#include "InterpolateNoData.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"
#include "HausdorffDistance.h"
#include "CentroidDistance.h"
#include <ogr_geometry.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

Difference<float>* generateCanopyHeightModel(const std::string&, const std::string&, const std::string&,
                                             CloudTools::IO::Reporter* reporter, po::variables_map& vm,
                                             RasterMetadata&);

GDALDataset* interpolateNoData(GDALDataset*, const std::string&, const float&,
																									CloudTools::IO::Reporter* reporter, po::variables_map& vm);

int countLocalMaximums(GDALDataset*, CloudTools::IO::Reporter*, po::variables_map&);

GDALDataset* antialias(Difference<float>*, const std::string&, CloudTools::IO::Reporter*, po::variables_map&);

GDALDataset* eliminateNonTrees(GDALDataset*, double, CloudTools::IO::Reporter*,
                                                  po::variables_map&);

std::vector<OGRPoint> collectSeedPoints(GDALDataset* target, CloudTools::IO::Reporter*,
                                        po::variables_map&);

ClusterMap treeCrownSegmentation(GDALDataset*, std::vector<OGRPoint>&,
                                             CloudTools::IO::Reporter*, po::variables_map&);

ClusterMap morphologyFiltering(GDALDataset*, const MorphologyClusterFilter::Method,
                                             ClusterMap&,
                                             const std::string&, int threshold = -1);

HausdorffDistance* calculateHausdorffDistance(ClusterMap&, ClusterMap&);

CentroidDistance* calculateGravityDistance(ClusterMap&, ClusterMap&);

ClusterMap createRefinedClusterMap(
	int, const std::string&, const std::string&, const std::string&, RasterMetadata& targetMetadata,
	CloudTools::IO::Reporter*, po::variables_map&);

void calculateHeightDifference(ClusterMap&, ClusterMap&, HausdorffDistance*);

void calculateVolumeDifference(ClusterMap&, ClusterMap&, HausdorffDistance*);

void writeClusterCentersToFile(ClusterMap&, ClusterMap&, RasterMetadata& targetMetadata,
                            const std::string&, HausdorffDistance*);

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
                             const std::string& ahn2Outpath, HausdorffDistance* distance);


void calculateHeightDifference(ClusterMap&, ClusterMap&, CentroidDistance*);

void calculateVolumeDifference(ClusterMap&, ClusterMap&, CentroidDistance*);

void writeClusterCentersToFile(ClusterMap&, ClusterMap&, RasterMetadata& targetMetadata,
                            const std::string&, CentroidDistance*);

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
                             const std::string& ahn2Outpath, CentroidDistance* distance);

void writeClusterMapToFile(const ClusterMap& cluster, const RasterMetadata& metadata, const std::string& outpath);

void writeClustesHeightsToFile(
	ClusterMap& ahn2Map, ClusterMap& ahn3Map,
	const std::string& ahn2DSM, const std::string& ahn3DSM,
	const std::string& outpath, CentroidDistance* distance,
	CloudTools::IO::Reporter* reporter, po::variables_map& vm);

int main(int argc, char* argv[])
{
	std::string DTMinputPath;
	std::string DSMinputPath;
	std::string AHN2DSMinputPath;
	std::string AHN2DTMinputPath;
	std::string outputDir = fs::current_path().string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn3-dtm-input-path,t", po::value<std::string>(&DTMinputPath), "DTM input path")
		("ahn3-dsm-input-path,s", po::value<std::string>(&DSMinputPath), "DSM input path")
		("ahn2-dtm-input-path,y", po::value<std::string>(&AHN2DTMinputPath), "AHN2 DTM input path")
		("ahn2-dsm-input-path,x", po::value<std::string>(&AHN2DSMinputPath), "AHN2 DSM input path")
		("output-dir,o", po::value<std::string>(&outputDir)->default_value(outputDir), "result directory path")
		("hausdorff-distance,d", "use Hausdorff-distance")
		("parallel,p", "parallel execution for AHN-2 and AHN-3") // TODO: this will mess up the log output
		("verbose,v", "verbose output")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares an AHN-2 and AHN-3 tile pair and filters out changes in vegtation." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("ahn3-dsm-input-path"))
	{
		std::cerr << "AHN-3 surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn3-dtm-input-path"))
	{
		std::cerr << "AHN-3 terrain input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn2-dsm-input-path"))
	{
		std::cerr << "AHN-2 surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn2-dtm-input-path"))
	{
		std::cerr << "AHN-2 terrain input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!fs::exists(DSMinputPath))
	{
		std::cerr << "The surface input file does not exist." << std::endl;
		argumentError = true;
	}
	if (!fs::exists(DTMinputPath))
	{
		std::cerr << "The terrain input file does not exist." << std::endl;
		argumentError = true;
	}

	if (fs::exists(outputDir) && !fs::is_directory(outputDir))
	{
		std::cerr << "The given output path exists but is not a directory." << std::endl;
		argumentError = true;
	}
	else if (!fs::exists(outputDir) && !fs::create_directory(outputDir))
	{
		std::cerr << "Failed to create output directory." << std::endl;
		argumentError = true;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	Reporter* reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	if (!vm.count("quiet"))
		std::cout << "=== AHN Vegetation Filter ===" << std::endl;

	GDALAllRegister();

	RasterMetadata targetMetadata, targetMetadata_dummy;

	std::future<ClusterMap> ahn3Future =
		std::async(
			vm.count("parallel") ? std::launch::async : std::launch::deferred,
			createRefinedClusterMap, 3, DTMinputPath, DSMinputPath, outputDir, std::ref(targetMetadata), reporter, std::ref(vm));

	std::future<ClusterMap> ahn2Future =
		std::async(
			vm.count("parallel") ? std::launch::async : std::launch::deferred,
			createRefinedClusterMap, 2, AHN2DTMinputPath, AHN2DSMinputPath, outputDir, std::ref(targetMetadata_dummy), reporter, std::ref(vm));

	// wait for the async results ...
	ClusterMap ahn3Pair = ahn3Future.get();
	ClusterMap ahn2Pair = ahn2Future.get();

	int pairs, lonelyAHN2, lonelyAHN3;

	if (vm.count("hausdorff-distance"))
	{
		std::cout << "Using Hausdorff distance to pair up clusters." << std::endl;
		HausdorffDistance* distance = calculateHausdorffDistance(ahn2Pair, ahn3Pair);
		std::cout << "Hausdorff distance calculated." << std::endl;

		pairs = distance->closest().size();
		lonelyAHN2 = distance->lonelyAHN2().size();
		lonelyAHN3 = distance->lonelyAHN3().size();

		writeClusterCentersToFile(ahn2Pair, ahn3Pair, targetMetadata,
			(fs::path(outputDir) / "cluster_centers.tif").string(), distance);
		writeClusterPairsToFile(ahn2Pair, ahn3Pair, targetMetadata,
			(fs::path(outputDir) / "cluster_pairs.tif").string(), distance);
		calculateHeightDifference(ahn2Pair, ahn3Pair, distance);
		calculateVolumeDifference(ahn2Pair, ahn3Pair, distance);
	}
	else
	{
		std::cout << "Using gravity distance to pair up clusters." << std::endl;
		CentroidDistance* distance =
			calculateGravityDistance(ahn2Pair, ahn3Pair);
		std::cout << "Gravity distance calculated." << std::endl;

		pairs = distance->closest().size();
		lonelyAHN2 = distance->lonelyAHN2().size();
		lonelyAHN3 = distance->lonelyAHN3().size();

		writeClusterCentersToFile(ahn2Pair, ahn3Pair, targetMetadata,
			(fs::path(outputDir) / "cluster_map.tif").string(), distance);
		writeClusterPairsToFile(ahn2Pair, ahn3Pair, targetMetadata,
			(fs::path(outputDir) / "cluster_pairs.tif").string(), distance);
		calculateHeightDifference(ahn2Pair, ahn3Pair, distance);
		calculateVolumeDifference(ahn2Pair, ahn3Pair, distance);

		writeClustesHeightsToFile(
			ahn2Pair, ahn3Pair,
			AHN2DSMinputPath, DSMinputPath,
			(fs::path(outputDir) / "cluster_height.tif").string(),
			distance,
			reporter, vm);
	}

	std::cout << "Total number of clusters in AHN2: " << ahn2Pair.clusterIndexes().size() << std::
		endl;
	std::cout << "Total number of clusters in AHN3: " << ahn3Pair.clusterIndexes().size() << std::
		endl;
	std::cout << "Pairs found: " << pairs << std::endl;
	std::cout << "Number of unpaired clusters in AHN2: " << lonelyAHN2 << std::endl;
	std::cout << "Number of unpaired clusters in AHN3: " << lonelyAHN3 << std::endl;

	delete reporter;
	return Success;
}

bool reporter(CloudTools::DEM::SweepLineTransformation<float>* transformation, CloudTools::IO::Reporter* reporter)
{
	transformation->progress = [&reporter](float complete, const std::string& message)
	{
		reporter->report(complete, message);
		return true;
	};
	return false;
}

// Generate canopy height model of given DSM and DTM.
Difference<float>* generateCanopyHeightModel(const std::string& DTMinput, const std::string& DSMinput,
                                             const std::string& outpath,
                                             CloudTools::IO::Reporter* reporter, po::variables_map& vm,
                                             RasterMetadata& metadata)
{
	Difference<float>* comparison = new Difference<float>({DTMinput, DSMinput}, outpath);
	if (!vm.count("quiet"))
	{
		comparison->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	comparison->execute();
	reporter->reset();

	metadata = comparison->targetMetadata();
	return comparison;
}

// Fill nodata points with data interpolated from neighboring points.
GDALDataset* interpolateNoData(GDALDataset* chm, const std::string& outpath, const float& threshold,
																									CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	InterpolateNoData interpolation({chm}, outpath);

  if (!vm.count("quiet"))
	{
		interpolation.progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
  reporter->reset();
  interpolation.execute();

  return interpolation.target();
}

// Count local maximum points in CHM.
int countLocalMaximums(GDALDataset* input, CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	SweepLineCalculation<float>* countLocalMax = new SweepLineCalculation<float>(
		{input}, 1, nullptr);

	int counter = 0;
	countLocalMax->computation = [&countLocalMax, &counter](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData())
			return;

		for (int i = -countLocalMax->range(); i <= countLocalMax->range(); i++)
			for (int j = -countLocalMax->range(); j <= countLocalMax->range(); j++)
				if (source.data(i, j) > source.data(0, 0))
					return;
		++counter;
	};

	if (!vm.count("quiet"))
	{
		countLocalMax->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	countLocalMax->execute();
	delete countLocalMax;

	return counter;
}

// Perform anti-aliasing on CHM using a convolution matrix.
GDALDataset* antialias(Difference<float>* target, const std::string& outpath,
                                CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	MatrixTransformation filter(target->target(), outpath, 1);
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
		filter.progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	filter.execute();
	return filter.target();
}

// Eliminate points that are smaller than a possible tree.
GDALDataset* eliminateNonTrees(GDALDataset* target, double threshold, const std::string& outpath,
                                                  CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	SweepLineTransformation<float> eliminateNonTrees({target}, outpath, 0, nullptr);

	eliminateNonTrees.computation = [&eliminateNonTrees, &threshold](int x, int y,
	                                                                  const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData() || source.data() < threshold)
			return static_cast<float>(eliminateNonTrees.nodataValue);
		else
			return source.data();
	};

	if (!vm.count("quiet"))
	{
		eliminateNonTrees.progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	eliminateNonTrees.execute();
	return eliminateNonTrees.target();
}

// Collect remaining local maximum points
std::vector<OGRPoint> collectSeedPoints(GDALDataset* target, CloudTools::IO::Reporter* reporter,
                                        po::variables_map& vm)
{
	SweepLineCalculation<float>* collectSeeds = new SweepLineCalculation<float>(
		{target}, 7, nullptr);

	std::vector<OGRPoint> seedPoints;
	collectSeeds->computation = [&collectSeeds, &seedPoints](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData())
			return;

		for (int i = -collectSeeds->range(); i <= collectSeeds->range(); i++)
			for (int j = -collectSeeds->range(); j <= collectSeeds->range(); j++)
				if (source.data(i, j) > source.data(0, 0))
					return;
		seedPoints.emplace_back(x, y, source.data(0, 0));
	};

	if (!vm.count("quiet"))
	{
		collectSeeds->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	collectSeeds->execute();
	delete collectSeeds;
	return seedPoints;
}

// Create a cluster map of the remaining points using the collected seed points.
ClusterMap treeCrownSegmentation(GDALDataset* target, std::vector<OGRPoint>& seedPoints,
                                             CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	TreeCrownSegmentation* crownSegmentation = new TreeCrownSegmentation({target}, seedPoints, nullptr);
	if (!vm.count("quiet"))
	{
		crownSegmentation->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	crownSegmentation->execute();

	return crownSegmentation->clusterMap();
}

// Perform morphological filtering (erosion or dilation) on a previously built cluster map.
ClusterMap morphologyFiltering(GDALDataset* target, const MorphologyClusterFilter::Method method,
																ClusterMap& clusterMap, const std::string& outpath, int threshold)
{
	MorphologyClusterFilter morphologyFilter(clusterMap, {target}, nullptr, method, nullptr);
	morphologyFilter.threshold = threshold;
	morphologyFilter.execute();

	return morphologyFilter.clusterMap;
}

// Calculate the Hausdorff-distance of two cluster maps.
HausdorffDistance* calculateHausdorffDistance(ClusterMap& ahn2ClusterMap, ClusterMap& ahn3ClusterMap)
{
	HausdorffDistance* distance = new HausdorffDistance(ahn2ClusterMap, ahn3ClusterMap);
	distance->execute();
	return distance;
}

// Calculate the Hausdorff-distance of two cluster maps.
CentroidDistance* calculateGravityDistance(ClusterMap& ahn2ClusterMap, ClusterMap& ahn3ClusterMap)
{
	CentroidDistance* distance = new CentroidDistance(ahn2ClusterMap, ahn3ClusterMap);
	distance->execute();
	return distance;
}

// Perform the algorithm on a DSM and a DTM.
ClusterMap createRefinedClusterMap(int ahnVersion, const std::string& DTMinput, const std::string& DSMinput,
	const std::string& outputDir, RasterMetadata& targetMetadata, CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	std::string chmOut, antialiasOut, nosmallOut, interpolOut, segmentationOut, morphologyOut;
	if (ahnVersion == 3)
	{
		chmOut = (fs::path(outputDir) / "ahn3_CHM.tif").string();
		antialiasOut = (fs::path(outputDir) / "ahn3_antialias.tif").string();
		nosmallOut = (fs::path(outputDir) / "ahn3_nosmall.tif").string();
		interpolOut = (fs::path(outputDir) / "ahn3_interpol.tif").string();
		segmentationOut = (fs::path(outputDir) / "ahn3_segmentation.tif").string();
		morphologyOut = (fs::path(outputDir) / "ahn3_morphology.tif").string();
	}
	else
	{
		chmOut = (fs::path(outputDir) / "ahn2_CHM.tif").string();
		antialiasOut = (fs::path(outputDir) / "ahn2_antialias.tif").string();
		nosmallOut = (fs::path(outputDir) / "ahn2_nosmall.tif").string();
		interpolOut = (fs::path(outputDir) / "ahn2_interpol.tif").string();
		segmentationOut = (fs::path(outputDir) / "ahn2_segmentation.tif").string();
		morphologyOut = (fs::path(outputDir) / "ahn2_morphology.tif").string();
	}

	Difference<float>* CHM = generateCanopyHeightModel(DTMinput, DSMinput, chmOut, reporter, vm, targetMetadata);
	std::cout << "CHM generated." << std::endl;

	// This step is optional.
	int counter = countLocalMaximums(CHM->target(), reporter, vm);
	std::cout << "Number of local maximums are: " << counter << std::endl;

	GDALDataset* filter = antialias(CHM, antialiasOut, reporter, vm);
	std::cout << "Matrix transformation performed." << std::endl;

	delete CHM;

	double treeHeightThreshold = 1.5;
	GDALDataset* onlyTrees = eliminateNonTrees(filter, treeHeightThreshold, nosmallOut, reporter, vm);
	std::cout << "Too small values eliminated." << std::endl;

  GDALDataset* interpolatedCHM = interpolateNoData(onlyTrees, interpolOut, 0.5, reporter, vm);
  std::cout << "Nodata points filled with interpolated data." << std::endl;

	// This step is optional.
	counter = countLocalMaximums(interpolatedCHM, reporter, vm);
	std::cout << "Number of local maximums are: " << counter << std::endl;

	delete filter;
  //delete interpolatedCHM;

	// Count & collect local maximum values
	std::vector<OGRPoint> seedPoints = collectSeedPoints(interpolatedCHM, reporter, vm);
	std::cout << "Seed points collected" << std::endl;

	ClusterMap cluster;
	cluster.setSizeX(targetMetadata.rasterSizeX());
	cluster.setSizeY(targetMetadata.rasterSizeY());

	cluster = treeCrownSegmentation(interpolatedCHM, seedPoints, reporter, vm);
	std::cout << "Tree crown segmentation performed." << std::endl;
	std::cout << "clusterindex number: " << cluster.clusterIndexes().size() << std::endl;

	writeClusterMapToFile(
		cluster,
		targetMetadata,
		segmentationOut);

	// Perform morphological erosion on the cluster map
	int erosionThreshold = 6, filterCounter = 3;

	for (int i = 0; i < filterCounter; ++i)
	{
		cluster = morphologyFiltering(interpolatedCHM, MorphologyClusterFilter::Method::Erosion, cluster,
			                            std::string(), erosionThreshold);
		std::cout << "Morphological erosion performed." << std::endl;

		cluster = morphologyFiltering(interpolatedCHM, MorphologyClusterFilter::Method::Dilation, cluster, std::string());
		std::cout << "Morphological dilation performed." << std::endl;
	}

	cluster.removeSmallClusters(16);

	writeClusterMapToFile(
		cluster,
		targetMetadata,
		morphologyOut);

	delete onlyTrees;
	delete interpolatedCHM;

	return cluster;
}

// Write the result of Hausdorff-distance calculation to geotiff file.
void writeClusterCentersToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
                            const std::string& ahn2Outpath, HausdorffDistance* distance)
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

	int commonId;
	CPLErr ioResult = CE_None;
	for (auto elem : distance->closest())
	{
		commonId = 1;
		auto center = ahn2Map.center(elem.first.first);
		ioResult = static_cast<CPLErr>(ioResult |
			targetBand->RasterIO(GF_Write,
			                     center.getX(), center.getY(),
			                     1, 1,
			                     &commonId,
			                     1, 1,
			                     gdalType<int>(),
			                     0, 0));

		center = ahn3Map.center(elem.first.second);
		ioResult = static_cast<CPLErr>(ioResult |
			targetBand->RasterIO(GF_Write,
			                     center.getX(), center.getY(),
			                     1, 1,
			                     &commonId,
			                     1, 1,
			                     gdalType<int>(),
			                     0, 0));
	}

	for (auto elem : distance->lonelyAHN2())
	{
		commonId = 2;
		auto center = ahn2Map.center(elem);
		ioResult = static_cast<CPLErr>(ioResult |
			targetBand->RasterIO(GF_Write,
			                     center.getX(), center.getY(),
			                     1, 1,
			                     &commonId,
			                     1, 1,
			                     gdalType<int>(),
			                     0, 0));
	}

	for (auto elem : distance->lonelyAHN3())
	{
		commonId = 3;
		auto center = ahn3Map.center(elem);
		ioResult = static_cast<CPLErr>(ioResult |
			targetBand->RasterIO(GF_Write,
			                     center.getX(), center.getY(),
			                     1, 1,
			                     &commonId,
			                     1, 1,
			                     gdalType<int>(),
			                     0, 0));
	}

	if (ioResult != CE_None)
		throw std::runtime_error("Target write error occured.");

	GDALClose(target);
}

void calculateHeightDifference(ClusterMap& ahn2, ClusterMap& ahn3, HausdorffDistance* distance)
{
	std::map<std::pair<GUInt32, GUInt32>, double> differences;
	OGRPoint ahn2Highest, ahn3Highest;
	double diff;
	for (const auto& elem : distance->closest())
	{
		ahn2Highest = ahn2.highestPoint(elem.first.first);
		ahn3Highest = ahn3.highestPoint(elem.first.second);

		diff = ahn3Highest.getZ() - ahn2Highest.getZ();
		differences.insert(std::make_pair(elem.first, diff));
		//std::cout << elem.first.first << ", " << elem.first.second << ": " << diff << std::endl;
	}
}

void calculateVolumeDifference(ClusterMap& ahn2, ClusterMap& ahn3, HausdorffDistance* distance)
{
	std::map<GUInt32, double> ahn2LonelyVolume, ahn3LonelyVolume;
	double ahn2FullVolume = 0.0, ahn3FullVolume = 0.0;
	for (const auto& elem : distance->lonelyAHN2())
	{
		double volume = std::accumulate(ahn2.points(elem).begin(), ahn2.points(elem).end(),
		                                0.0, [](double sum, const OGRPoint& point)
		                                {
			                                return sum + point.getZ();
		                                });
		volume *= 0.25;
		ahn2LonelyVolume.insert(std::make_pair(elem, volume));
		ahn2FullVolume += std::abs(volume);
	}

	for (const auto& elem : distance->lonelyAHN3())
	{
		double volume = std::accumulate(ahn3.points(elem).begin(), ahn3.points(elem).end(),
		                                0.0, [](double sum, const OGRPoint& point)
		                                {
			                                return sum + point.getZ();
		                                });
		volume *= 0.25;
		ahn3LonelyVolume.insert(std::make_pair(elem, volume));
		ahn3FullVolume += std::abs(volume);
	}

	double ahn2ClusterVolume = 0.0, ahn3ClusterVolume = 0.0;
	std::map<std::pair<GUInt32, GUInt32>, double> diffs;
	for (const auto& elem : distance->closest())
	{
		ahn2ClusterVolume = std::accumulate(ahn2.points(elem.first.first).begin(),
		                                    ahn2.points(elem.first.first).end(), 0.0,
		                                    [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		ahn2ClusterVolume *= 0.25;
		ahn2FullVolume += std::abs(ahn2ClusterVolume);

		ahn3ClusterVolume = std::accumulate(ahn3.points(elem.first.second).begin(),
		                                    ahn3.points(elem.first.second).end(), 0.0,
		                                    [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		ahn3ClusterVolume *= 0.25;
		ahn3FullVolume += std::abs(ahn3ClusterVolume);

		diffs.insert(std::make_pair(elem.first, ahn3ClusterVolume - ahn2ClusterVolume));
		//std::cout << elem.first.first << ", " << elem.first.second << ": " << ahn3ClusterVolume - ahn2ClusterVolume <<
			//std::endl;
	}

	std::cout << "ahn2 full volume: " << ahn2FullVolume << std::endl;
	std::cout << "ahn3 full volume: " << ahn3FullVolume << std::endl;
	std::cout << "ahn2 and ahn3 difference: " << (ahn3FullVolume - ahn2FullVolume) << std::endl;
}

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
                             const std::string& ahn2Outpath, HausdorffDistance* distance)
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


// Write the result of Hausdorff-distance calculation to geotiff file.
void writeClusterCentersToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& metadata,
                            const std::string& ahn2Outpath, CentroidDistance* distance)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(ahn2Outpath) &&
		driver->Delete(ahn2Outpath.c_str()) == CE_Failure &&
		!fs::remove(ahn2Outpath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* target = driver->Create(ahn2Outpath.c_str(),
	                                     metadata.rasterSizeX(),
	                                     metadata.rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&metadata.geoTransform()[0]);

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	int commonId;
	CPLErr ioResult = CE_None;
	for (auto elem : distance->closest())
	{
		commonId = 1;
		auto center = ahn2Map.center(elem.first.first);
		ioResult = static_cast<CPLErr>(ioResult |
			targetBand->RasterIO(GF_Write,
			                     center.getX(), center.getY(),
			                     1, 1,
			                     &commonId,
			                     1, 1,
			                     gdalType<int>(),
			                     0, 0));

		center = ahn3Map.center(elem.first.second);
		ioResult = static_cast<CPLErr>(ioResult | targetBand->RasterIO(GF_Write,
		                                                               center.getX(), center.getY(),
		                                                               1, 1,
		                                                               &commonId,
		                                                               1, 1,
		                                                               gdalType<int>(),
		                                                               0, 0));
	}

	for (auto elem : distance->lonelyAHN2())
	{
		commonId = 2;
		auto center = ahn2Map.center(elem);
		ioResult = static_cast<CPLErr>(ioResult | targetBand->RasterIO(GF_Write,
		                                                               center.getX(), center.getY(),
		                                                               1, 1,
		                                                               &commonId,
		                                                               1, 1,
		                                                               gdalType<int>(),
		                                                               0, 0));
	}

	for (auto elem : distance->lonelyAHN3())
	{
		commonId = 3;
		auto center = ahn3Map.center(elem);
		ioResult = static_cast<CPLErr>(ioResult | targetBand->RasterIO(GF_Write,
		                                                               center.getX(), center.getY(),
		                                                               1, 1,
		                                                               &commonId,
		                                                               1, 1,
		                                                               gdalType<int>(),
		                                                               0, 0));
	}

	if (ioResult != CE_None)
		throw std::runtime_error("Target write error occured.");

	GDALClose(target);
}

void calculateHeightDifference(ClusterMap& ahn2, ClusterMap& ahn3, CentroidDistance* distance)
{
	std::map<std::pair<GUInt32, GUInt32>, double> differences;
	OGRPoint ahn2Highest, ahn3Highest;
	double diff;
	for (const auto& elem : distance->closest())
	{
		ahn2Highest = ahn2.highestPoint(elem.first.first);
		ahn3Highest = ahn3.highestPoint(elem.first.second);

		diff = ahn3Highest.getZ() - ahn2Highest.getZ();
		differences.insert(std::make_pair(elem.first, diff));
		std::cout << elem.first.first << ", " << elem.first.second << ": " << diff << std::endl;
	}
}

void calculateVolumeDifference(ClusterMap& ahn2, ClusterMap& ahn3, CentroidDistance* distance)
{
	std::map<GUInt32, double> ahn2LonelyVolume, ahn3LonelyVolume;
	double ahn2FullVolume = 0.0, ahn3FullVolume = 0.0;
	for (const auto& elem : distance->lonelyAHN2())
	{
		double volume = std::accumulate(ahn2.points(elem).begin(), ahn2.points(elem).end(),
		                                0.0, [](double sum, const OGRPoint& point)
		                                {
			                                return sum + point.getZ();
		                                });
		volume *= 0.25;
		ahn2LonelyVolume.insert(std::make_pair(elem, volume));
		ahn2FullVolume += std::abs(volume);
	}

	for (const auto& elem : distance->lonelyAHN3())
	{
		double volume = std::accumulate(ahn3.points(elem).begin(), ahn3.points(elem).end(),
		                                0.0, [](double sum, const OGRPoint& point)
		                                {
			                                return sum + point.getZ();
		                                });
		volume *= 0.25;
		ahn3LonelyVolume.insert(std::make_pair(elem, volume));
		ahn3FullVolume += std::abs(volume);
	}

	double ahn2ClusterVolume = 0.0, ahn3ClusterVolume = 0.0;
	std::map<std::pair<GUInt32, GUInt32>, double> diffs;
	for (const auto& elem : distance->closest())
	{
		ahn2ClusterVolume = std::accumulate(ahn2.points(elem.first.first).begin(),
		                                    ahn2.points(elem.first.first).end(), 0.0,
		                                    [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		ahn2ClusterVolume *= 0.25;
		ahn2FullVolume += std::abs(ahn2ClusterVolume);

		ahn3ClusterVolume = std::accumulate(ahn3.points(elem.first.second).begin(),
		                                    ahn3.points(elem.first.second).end(), 0.0,
		                                    [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		ahn3ClusterVolume *= 0.25;
		ahn3FullVolume += std::abs(ahn3ClusterVolume);

		diffs.insert(std::make_pair(elem.first, ahn3ClusterVolume - ahn2ClusterVolume));
		//std::cout << elem.first.first << ", " << elem.first.second << ": " << ahn3ClusterVolume - ahn2ClusterVolume <<
			//std::endl;
	}

	std::cout << "ahn2 full volume: " << ahn2FullVolume << std::endl;
	std::cout << "ahn3 full volume: " << ahn3FullVolume << std::endl;
	std::cout << "ahn2 and ahn3 difference: " << (ahn3FullVolume - ahn2FullVolume) << std::endl;
}

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
                             const std::string& ahn2Outpath, CentroidDistance* distance)
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
			ioResult = static_cast<CPLErr>(ioResult | targetBand->RasterIO(GF_Write,
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

void writeClusterMapToFile(const ClusterMap& cluster, const RasterMetadata& metadata, const std::string& outpath)
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

void writeClustesHeightsToFile(
	ClusterMap& ahn2Map, ClusterMap& ahn3Map,
	const std::string& ahn2DSM, const std::string& ahn3DSM,
	const std::string& outpath, CentroidDistance* distance,
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
