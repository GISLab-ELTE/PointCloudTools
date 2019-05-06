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

#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"
#include "HausdorffDistance.h"
#include "CentroidDistance.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

Difference<float>* generateCanopyHeightModel(const std::string&, const std::string&, const std::string&,
                                             CloudTools::IO::Reporter* reporter, po::variables_map& vm);

int countLocalMaximums(GDALDataset*, CloudTools::IO::Reporter*, po::variables_map&);

MatrixTransformation* antialias(Difference<float>*, const std::string&, CloudTools::IO::Reporter*, po::variables_map&);

SweepLineTransformation<float>* eliminateNonTrees(MatrixTransformation*, double, CloudTools::IO::Reporter*,
                                                  po::variables_map&);

std::vector<OGRPoint> collectSeedPoints(SweepLineTransformation<float>* target, CloudTools::IO::Reporter*,
                                        po::variables_map&);

TreeCrownSegmentation* treeCrownSegmentation(SweepLineTransformation<float>*, std::vector<OGRPoint>&,
                                             CloudTools::IO::Reporter*, po::variables_map&);

MorphologyClusterFilter* morphologyFiltering(SweepLineTransformation<float>*, const MorphologyClusterFilter::Method,
                                             ClusterMap&,
                                             const std::string&, int threshold = -1);

HausdorffDistance* calculateHausdorffDistance(ClusterMap&, ClusterMap&);

CentroidDistance* calculateGravityDistance(ClusterMap&, ClusterMap&);

std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> createRefinedClusterMap(
	int, const std::string&, const std::string&, const std::string&,
	CloudTools::IO::Reporter*, po::variables_map&);

void calculateHeightDifference(ClusterMap&, ClusterMap&, HausdorffDistance*);

void calculateVolumeDifference(ClusterMap&, ClusterMap&, HausdorffDistance*);

void writeClusterCentersToFile(ClusterMap&, ClusterMap&, TreeCrownSegmentation*,
                            const std::string&, HausdorffDistance*);

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
                             const std::string& ahn2Outpath, HausdorffDistance* distance);


void calculateHeightDifference(ClusterMap&, ClusterMap&, CentroidDistance*);

void calculateVolumeDifference(ClusterMap&, ClusterMap&, CentroidDistance*);

void writeClusterCentersToFile(ClusterMap&, ClusterMap&, TreeCrownSegmentation*,
                            const std::string&, CentroidDistance*);

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
                             const std::string& ahn2Outpath, CentroidDistance* distance);

void writeClusterMapToFile(const ClusterMap& cluster, const RasterMetadata& metadata, const std::string& outpath);

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

	std::future<std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*>> ahn3Future =
		std::async(
			vm.count("parallel") ? std::launch::async : std::launch::deferred,
			createRefinedClusterMap, 3, DTMinputPath, DSMinputPath, outputDir, reporter, std::ref(vm));

	std::future<std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*>> ahn2Future =
		std::async(
			vm.count("parallel") ? std::launch::async : std::launch::deferred,
			createRefinedClusterMap, 2, AHN2DTMinputPath, AHN2DSMinputPath, outputDir, reporter, std::ref(vm));

	// wait for the async results ...
	std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> ahn3Pair = ahn3Future.get();
	std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> ahn2Pair = ahn2Future.get();

	int pairs, lonelyAHN2, lonelyAHN3;

	if (vm.count("hausdorff-distance"))
	{
		std::cout << "Using Hausdorff distance to pair up clusters." << std::endl;
		HausdorffDistance* distance = calculateHausdorffDistance(ahn2Pair.first->clusterMap,
			ahn3Pair.first->clusterMap);
		std::cout << "Hausdorff distance calculated." << std::endl;
		pairs = distance->closest().size();
		lonelyAHN2 = distance->lonelyAHN2().size();
		lonelyAHN3 = distance->lonelyAHN3().size();

		writeClusterCentersToFile(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, ahn2Pair.second,
			(fs::path(outputDir) / "cluster_centers.tif").string(), distance);
		writeClusterPairsToFile(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, ahn2Pair.second,
			(fs::path(outputDir) / "cluster_pairs.tif").string(), distance);
		calculateHeightDifference(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, distance);
		calculateVolumeDifference(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, distance);
	}
	else
	{
		std::cout << "Using gravity distance to pair up clusters." << std::endl;
		CentroidDistance* distance =
			calculateGravityDistance(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap);
		distance->execute();
		/*for (auto elem : distance->lonelyAHN2())
		{
		  //std::cout << elem.first.first << "    " << elem.first.second << "    " << elem.second << std::endl;
		  std::cout << elem << std::endl;
		}
		std::cout << "AHN3 LONELY INDEXES" << std::endl;
		for (auto elem : distance->lonelyAHN3())
		{
		  //std::cout << elem.first.first << "    " << elem.first.second << "    " << elem.second << std::endl;
		  std::cout << elem << std::endl;
		}
		 */
		 //std::cout << "Hausdorff-distance calculated." << std::endl;
		std::cout << "Gravity distance calculated." << std::endl;
		pairs = distance->closest().size();
		lonelyAHN2 = distance->lonelyAHN2().size();
		lonelyAHN3 = distance->lonelyAHN3().size();
		writeClusterCentersToFile(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, ahn2Pair.second,
			(fs::path(outputDir) / "cluster_map.tif").string(), distance);
		writeClusterPairsToFile(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, ahn2Pair.second,
			(fs::path(outputDir) / "cluster_pairs.tif").string(), distance);
		calculateHeightDifference(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, distance);
		calculateVolumeDifference(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, distance);
	}

	std::cout << "Total number of clusters in AHN2: " << ahn2Pair.first->clusterMap.clusterIndexes().size() << std::
		endl;
	std::cout << "Total number of clusters in AHN3: " << ahn3Pair.first->clusterMap.clusterIndexes().size() << std::
		endl;
	std::cout << "Pairs found: " << pairs << std::endl;
	std::cout << "Number of unpaired clusters in AHN2: " << lonelyAHN2 << std::endl;
	std::cout << "Number of unpaired clusters in AHN3: " << lonelyAHN3 << std::endl;

	delete ahn3Pair.second;
	delete ahn3Pair.first;
	delete ahn2Pair.second;
	delete ahn2Pair.first;

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
                                             CloudTools::IO::Reporter* reporter, po::variables_map& vm)
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
	return comparison;
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

	return counter;
}

// Perform anti-aliasing on CHM using a convolution matrix.
MatrixTransformation* antialias(Difference<float>* target, const std::string& outpath,
                                CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	MatrixTransformation* filter = new MatrixTransformation(target->target(), outpath, 1);
	filter->setMatrix(0, 0, 4); // middle
	filter->setMatrix(0, -1, 2); // sides
	filter->setMatrix(0, 1, 2);
	filter->setMatrix(-1, 0, 2);
	filter->setMatrix(1, 0, 2);
	filter->setMatrix(-1, -1, 1); // corners
	filter->setMatrix(1, -1, 1);
	filter->setMatrix(-1, 1, 1);
	filter->setMatrix(1, 1, 1);

	if (!vm.count("quiet"))
	{
		filter->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	filter->execute();
	return filter;
}

// Eliminate points that are smaller than a possible tree.
SweepLineTransformation<float>* eliminateNonTrees(MatrixTransformation* target, double threshold, const std::string& outpath,
                                                  CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	SweepLineTransformation<float>* eliminateNonTrees = new SweepLineTransformation<float>(
		{target->target()}, outpath, 0, nullptr);

	eliminateNonTrees->computation = [&eliminateNonTrees, &threshold](int x, int y,
	                                                                  const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData() || source.data() < threshold)
			return static_cast<float>(eliminateNonTrees->nodataValue);
		else
			return source.data();
	};

	if (!vm.count("quiet"))
	{
		eliminateNonTrees->progress = [&reporter](float complete, const std::string& message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	eliminateNonTrees->execute();
	return eliminateNonTrees;
}

// Collect remaining local maximum points
std::vector<OGRPoint> collectSeedPoints(SweepLineTransformation<float>* target, CloudTools::IO::Reporter* reporter,
                                        po::variables_map& vm)
{
	SweepLineCalculation<float>* collectSeeds = new SweepLineCalculation<float>(
		{target->target()}, 7, nullptr);

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
TreeCrownSegmentation* treeCrownSegmentation(SweepLineTransformation<float>* target, std::vector<OGRPoint>& seedPoints,
                                             CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	TreeCrownSegmentation* crownSegmentation = new TreeCrownSegmentation(
		{target->target()}, seedPoints, nullptr);
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
	return crownSegmentation;
}

// Perform morphological filtering (erosion or dilation) on a previously built cluster map.
MorphologyClusterFilter* morphologyFiltering(SweepLineTransformation<float>* target,
                                             const MorphologyClusterFilter::Method method,
                                             ClusterMap& clusterMap, const std::string& outpath, int threshold)
{
	MorphologyClusterFilter* morphologyFilter = new MorphologyClusterFilter(
		clusterMap, {target->target()}, nullptr, method, nullptr);
	morphologyFilter->threshold = threshold;
	morphologyFilter->execute();
	return morphologyFilter;
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
std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> createRefinedClusterMap(
	int ahnVersion, const std::string& DTMinput, const std::string& DSMinput, const std::string& outputDir,
	CloudTools::IO::Reporter* reporter, po::variables_map& vm)
{
	std::string chmOut, antialiasOut, nosmallOut, segmentationOut, morphologyOut;
	if (ahnVersion == 3)
	{
		chmOut = (fs::path(outputDir) / "ahn3_CHM.tif").string();
		antialiasOut = (fs::path(outputDir) / "ahn3_antialias.tif").string();
		nosmallOut = (fs::path(outputDir) / "ahn3_nosmall.tif").string();
		segmentationOut = (fs::path(outputDir) / "ahn3_segmentation.tif").string();
		morphologyOut = (fs::path(outputDir) / "ahn3_morphology.tif").string();
	}
	else
	{
		chmOut = (fs::path(outputDir) / "ahn2_CHM.tif").string();
		antialiasOut = (fs::path(outputDir) / "ahn2_antialias.tif").string();
		nosmallOut = (fs::path(outputDir) / "ahn2_nosmall.tif").string();
		segmentationOut = (fs::path(outputDir) / "ahn2_segmentation.tif").string();
		morphologyOut = (fs::path(outputDir) / "ahn2_morphology.tif").string();
	}

	Difference<float>* CHM = generateCanopyHeightModel(DTMinput, DSMinput, chmOut, reporter, vm);
	std::cout << "CHM generated." << std::endl;

	// This step is optional.
	int counter = countLocalMaximums(CHM->target(), reporter, vm);
	std::cout << "Number of local maximums are: " << counter << std::endl;

	MatrixTransformation* filter = antialias(CHM, antialiasOut, reporter, vm);
	std::cout << "Matrix transformation performed." << std::endl;

	delete CHM;

	double treeHeightThreshold = 1.5;
	SweepLineTransformation<float>* onlyTrees = eliminateNonTrees(filter, treeHeightThreshold, nosmallOut, reporter, vm);
	std::cout << "Too small values eliminated." << std::endl;

	// This step is optional.
	counter = countLocalMaximums(onlyTrees->target(), reporter, vm);
	std::cout << "Number of local maximums are: " << counter << std::endl;

	delete filter;

	// Count & collect local maximum values
	std::vector<OGRPoint> seedPoints = collectSeedPoints(onlyTrees, reporter, vm);
	std::cout << "Seed points collected" << std::endl;

	ClusterMap cluster;
	cluster.setSizeX(onlyTrees->targetMetadata().rasterSizeX());
	cluster.setSizeY(onlyTrees->targetMetadata().rasterSizeY());

	TreeCrownSegmentation* crownSegmentation = treeCrownSegmentation(onlyTrees, seedPoints, reporter, vm);
	std::cout << "Tree crown segmentation performed." << std::endl;
	cluster = crownSegmentation->clusterMap();
	std::cout << "clusterindex number: " << cluster.clusterIndexes().size() << std::endl;

	writeClusterMapToFile(
		cluster,
		crownSegmentation->targetMetadata(),
		segmentationOut);

	// Perform morphological erosion on the cluster map
	int erosionThreshold = 6, filterCounter = 3;
	for (int i = 0; i < filterCounter - 1; ++i)
	{
		MorphologyClusterFilter* erosion = morphologyFiltering(onlyTrees,
		                                                       MorphologyClusterFilter::Method::Erosion, cluster,
		                                                       std::string(), erosionThreshold);
		std::cout << "Morphological erosion performed." << std::endl;

		MorphologyClusterFilter* dilation = morphologyFiltering(onlyTrees,
		                                                        MorphologyClusterFilter::Method::Dilation,
		                                                        erosion->clusterMap, std::string());
		std::cout << "Morphological dilation performed." << std::endl;

		cluster = dilation->clusterMap;

		delete erosion;
		delete dilation;
	}

	MorphologyClusterFilter* erosion = morphologyFiltering(onlyTrees,
	                                                       MorphologyClusterFilter::Method::Erosion, cluster,
	                                                       std::string(), erosionThreshold);
	std::cout << "Morphological erosion performed." << std::endl;

	MorphologyClusterFilter* dilation = morphologyFiltering(onlyTrees,
	                                                        MorphologyClusterFilter::Method::Dilation,
	                                                        erosion->clusterMap, std::string());
	std::cout << "Morphological dilation performed." << std::endl;

	dilation->clusterMap.removeSmallClusters(16);

	writeClusterMapToFile(
		dilation->clusterMap,
		dilation->targetMetadata(),
		morphologyOut);

	delete onlyTrees;
	delete erosion;

	return std::make_pair(dilation, crownSegmentation);
}

// Write the result of Hausdorff-distance calculation to geotiff file.
void writeClusterCentersToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
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
	                                     segmentation->targetMetadata().rasterSizeX(),
	                                     segmentation->targetMetadata().rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&segmentation->targetMetadata().geoTransform()[0]);

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	int commonId;
	for (auto elem : distance->closest())
	{
		commonId = 1;
		auto center = ahn2Map.center(elem.first.first);
		CPLErr ioResult = targetBand->RasterIO(GF_Write,
		                                       center.getX(), center.getY(),
		                                       1, 1,
		                                       &commonId,
		                                       1, 1,
		                                       gdalType<int>(),
		                                       0, 0);

		center = ahn3Map.center(elem.first.second);
		ioResult = targetBand->RasterIO(GF_Write,
		                                center.getX(), center.getY(),
		                                1, 1,
		                                &commonId,
		                                1, 1,
		                                gdalType<int>(),
		                                0, 0);

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occured.");
	}

	for (auto elem : distance->lonelyAHN2())
	{
		commonId = 2;
		auto center = ahn2Map.center(elem);
		CPLErr ioResult = targetBand->RasterIO(GF_Write,
		                                       center.getX(), center.getY(),
		                                       1, 1,
		                                       &commonId,
		                                       1, 1,
		                                       gdalType<int>(),
		                                       0, 0);
	}

	for (auto elem : distance->lonelyAHN3())
	{
		commonId = 3;
		auto center = ahn3Map.center(elem);
		CPLErr ioResult = targetBand->RasterIO(GF_Write,
		                                       center.getX(), center.getY(),
		                                       1, 1,
		                                       &commonId,
		                                       1, 1,
		                                       gdalType<int>(),
		                                       0, 0);
	}

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
		std::cout << elem.first.first << ", " << elem.first.second << ": " << ahn3ClusterVolume - ahn2ClusterVolume <<
			std::endl;
	}

	std::cout << "ahn2 full volume: " << ahn2FullVolume << std::endl;
	std::cout << "ahn3 full volume: " << ahn3FullVolume << std::endl;
	std::cout << "ahn2 and ahn3 difference: " << (ahn3FullVolume - ahn2FullVolume) << std::endl;
}

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
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
	                                     segmentation->targetMetadata().rasterSizeX(),
	                                     segmentation->targetMetadata().rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&segmentation->targetMetadata().geoTransform()[0]);

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
		CPLErr ioResult;
		for (const auto& point : points)
		{
			ioResult = targetBand->RasterIO(GF_Write,
			                                point.getX(), point.getY(),
			                                1, 1,
			                                &commonId,
			                                1, 1,
			                                gdalType<int>(),
			                                0, 0);
		}

		points = ahn3Map.points(elem.first.second);
		for (const auto& point : points)
		{
			ioResult = targetBand->RasterIO(GF_Write,
			                                point.getX(), point.getY(),
			                                1, 1,
			                                &commonId,
			                                1, 1,
			                                gdalType<int>(),
			                                0, 0);
		}

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occured.");
	}

	commonId = -2;
	for (auto elem : distance->lonelyAHN2())
	{
		points = ahn2Map.points(elem);
		for (const auto& point : points)
		{
			CPLErr ioResult = targetBand->RasterIO(GF_Write,
			                                       point.getX(), point.getY(),
			                                       1, 1,
			                                       &commonId,
			                                       1, 1,
			                                       gdalType<int>(),
			                                       0, 0);
		}
	}

	commonId = -3;
	for (auto elem : distance->lonelyAHN3())
	{
		points = ahn3Map.points(elem);
		for (const auto& point : points)
		{
			CPLErr ioResult = targetBand->RasterIO(GF_Write,
			                                       point.getX(), point.getY(),
			                                       1, 1,
			                                       &commonId,
			                                       1, 1,
			                                       gdalType<int>(),
			                                       0, 0);
		}
	}

	GDALClose(target);
}


// Write the result of Hausdorff-distance calculation to geotiff file.
void writeClusterCentersToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
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
	                                     segmentation->targetMetadata().rasterSizeX(),
	                                     segmentation->targetMetadata().rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&segmentation->targetMetadata().geoTransform()[0]);

	GDALRasterBand* targetBand = target->GetRasterBand(1);
	targetBand->SetNoDataValue(-1);

	int commonId;
	for (auto elem : distance->closest())
	{
		commonId = 1;
		auto center = ahn2Map.center(elem.first.first);
		CPLErr ioResult = targetBand->RasterIO(GF_Write,
		                                       center.getX(), center.getY(),
		                                       1, 1,
		                                       &commonId,
		                                       1, 1,
		                                       gdalType<int>(),
		                                       0, 0);

		center = ahn3Map.center(elem.first.second);
		ioResult = targetBand->RasterIO(GF_Write,
		                                center.getX(), center.getY(),
		                                1, 1,
		                                &commonId,
		                                1, 1,
		                                gdalType<int>(),
		                                0, 0);

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occured.");
	}

	for (auto elem : distance->lonelyAHN2())
	{
		commonId = 2;
		auto center = ahn2Map.center(elem);
		CPLErr ioResult = targetBand->RasterIO(GF_Write,
		                                       center.getX(), center.getY(),
		                                       1, 1,
		                                       &commonId,
		                                       1, 1,
		                                       gdalType<int>(),
		                                       0, 0);
	}

	for (auto elem : distance->lonelyAHN3())
	{
		commonId = 3;
		auto center = ahn3Map.center(elem);
		CPLErr ioResult = targetBand->RasterIO(GF_Write,
		                                       center.getX(), center.getY(),
		                                       1, 1,
		                                       &commonId,
		                                       1, 1,
		                                       gdalType<int>(),
		                                       0, 0);
	}

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
		//std::cout << elem.first.first << ", " << elem.first.second << ": " << diff << std::endl;
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
		std::cout << elem.first.first << ", " << elem.first.second << ": " << ahn3ClusterVolume - ahn2ClusterVolume <<
			std::endl;
	}

	std::cout << "ahn2 full volume: " << ahn2FullVolume << std::endl;
	std::cout << "ahn3 full volume: " << ahn3FullVolume << std::endl;
	std::cout << "ahn2 and ahn3 difference: " << (ahn3FullVolume - ahn2FullVolume) << std::endl;
}

void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
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
	                                     segmentation->targetMetadata().rasterSizeX(),
	                                     segmentation->targetMetadata().rasterSizeY(), 1,
	                                     gdalType<int>(), nullptr);
	if (target == nullptr)
		throw std::runtime_error("Target file creation failed.");

	target->SetGeoTransform(&segmentation->targetMetadata().geoTransform()[0]);

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
		CPLErr ioResult;
		for (const auto& point : points)
		{
			ioResult = targetBand->RasterIO(GF_Write,
			                                point.getX(), point.getY(),
			                                1, 1,
			                                &commonId,
			                                1, 1,
			                                gdalType<int>(),
			                                0, 0);
		}

		points = ahn3Map.points(elem.first.second);
		for (const auto& point : points)
		{
			ioResult = targetBand->RasterIO(GF_Write,
			                                point.getX(), point.getY(),
			                                1, 1,
			                                &commonId,
			                                1, 1,
			                                gdalType<int>(),
			                                0, 0);
		}

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occured.");
	}

	commonId = -2;
	for (auto elem : distance->lonelyAHN2())
	{
		points = ahn2Map.points(elem);
		for (const auto& point : points)
		{
			CPLErr ioResult = targetBand->RasterIO(GF_Write,
			                                       point.getX(), point.getY(),
			                                       1, 1,
			                                       &commonId,
			                                       1, 1,
			                                       gdalType<int>(),
			                                       0, 0);
		}
	}

	commonId = -3;
	for (auto elem : distance->lonelyAHN3())
	{
		points = ahn3Map.points(elem);
		for (const auto& point : points)
		{
			CPLErr ioResult = targetBand->RasterIO(GF_Write,
			                                       point.getX(), point.getY(),
			                                       1, 1,
			                                       &commonId,
			                                       1, 1,
			                                       gdalType<int>(),
			                                       0, 0);
		}
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
		CPLErr ioResult;
		for (const auto& point : points)
		{
			ioResult = targetBand->RasterIO(GF_Write,
				point.getX(), point.getY(),
				1, 1,
				&commonId,
				1, 1,
				gdalType<int>(),
				0, 0);
		}

		if (ioResult != CE_None)
			throw std::runtime_error("Target write error occured.");
	}

	GDALClose(target);
}
