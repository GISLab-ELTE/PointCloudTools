#include <iostream>
#include <string>
#include <utility>

#include <boost/program_options.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/Filters/MorphologyFilter.hpp>
#include "NoiseFilter.h"
#include "MatrixTransformation.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"
#include "HausdorffDistance.h"

namespace po = boost::program_options;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

Difference<float>* generateCanopyHeightModel(const std::string&, const std::string&, const std::string&,
                               CloudTools::IO::Reporter *reporter, po::variables_map& vm);

int countLocalMaximums(Difference<float>*, CloudTools::IO::Reporter*, po::variables_map&);

MatrixTransformation* antialias(Difference<float>*, const std::string&, CloudTools::IO::Reporter*, po::variables_map&);

SweepLineTransformation<float>* eliminateNonTrees(MatrixTransformation*, double, CloudTools::IO::Reporter*, po::variables_map&);

std::vector<OGRPoint> collectSeedPoints(SweepLineTransformation<float> *target, CloudTools::IO::Reporter*, po::variables_map&);

TreeCrownSegmentation* treeCrownSegmentation(SweepLineTransformation<float>*, std::vector<OGRPoint>&, CloudTools::IO::Reporter*, po::variables_map&);

MorphologyClusterFilter* morphologyFiltering(const MorphologyClusterFilter::Method, ClusterMap&,
                                             const std::string&, int threshold = -1);

HausdorffDistance* calculateHausdorffDistance(ClusterMap&, ClusterMap&);

std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> createRefinedClusterMap(int, const std::string&, const std::string&,
                                                 CloudTools::IO::Reporter*, po::variables_map&);

void writeClusterMapsToFile(ClusterMap&, ClusterMap&, TreeCrownSegmentation*,
                            const std::string&, HausdorffDistance*);

void writeFullClustersToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
                             const std::string& ahn2Outpath, HausdorffDistance* distance);

int main(int argc, char* argv[])
{
	std::string DTMinputPath;
	std::string DSMinputPath;
	std::string AHN2DSMinputPath;
	std::string AHN2DTMinputPath;
	std::string outputPath = (fs::current_path() / "out.tif").string();
	std::string AHN2outputPath = (fs::current_path() / "ahn2_out.tif").string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn3-dtm-input-path,t", po::value<std::string>(&DTMinputPath), "DTM input path")
		("ahn3-dsm-input-path,s", po::value<std::string>(&DSMinputPath), "DSM input path")
		("ahn2-dtm-input-path,y", po::value<std::string>(&AHN2DTMinputPath), "AHN2 DTM input path")
		("ahn2-dsm-input-path,x", po::value<std::string>(&AHN2DSMinputPath), "AHN2 DSM input path")
		("output-path,o", po::value<std::string>(&outputPath)->default_value(outputPath), "output path")
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
		std::cerr << "Surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn3-dtm-input-path"))
	{
		std::cerr << "Terrain input file is mandatory." << std::endl;
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

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	Reporter *reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	if (!vm.count("quiet"))
		std::cout << "=== AHN Vegetation Filter ===" << std::endl;

	GDALAllRegister();

  std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> ahn3Pair = createRefinedClusterMap(3, DTMinputPath, DSMinputPath, reporter, vm);

	if (vm.count("ahn2-dtm-input-path") && vm.count("ahn2-dsm-input-path"))
	{
	  std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> ahn2Pair = createRefinedClusterMap(2, AHN2DTMinputPath, AHN2DSMinputPath, reporter, vm);

		HausdorffDistance *distance = calculateHausdorffDistance(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap);
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
		std::cout << "Hausdorff-distance calculated." << std::endl;
    writeClusterMapsToFile(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, ahn2Pair.second, AHN2outputPath, distance);
    writeFullClustersToFile(ahn2Pair.first->clusterMap, ahn3Pair.first->clusterMap, ahn2Pair.second, "cluster_pairs.tif", distance);

    delete ahn3Pair.second;
    delete ahn3Pair.first;
    delete ahn2Pair.second;
    delete ahn2Pair.first;

		// Calculate differences between dimensions
/*    std::map<std::pair<GUInt32, GUInt32>, std::pair<double, double>> diff;
    for (const auto& elem : distance->closest())
    {
      auto centerAHN2 = clusterMap.center(elem.first.first);
      auto centerAHN3 = clusters.center(elem.first.second);
      double vertical = centerAHN3.getZ() - centerAHN2.getZ();
      diff.insert(std::make_pair(elem.first, std::make_pair(elem.second, vertical)));
      std::cout << elem.first.first << "; " << elem.first.second << " horizontal distance: " << elem.second << ", vertical distance: " << vertical << std::endl;
      if(vertical == 23) {
        std::cout << "centerAHN2 X: " << centerAHN2.getX() << std::endl;
        std::cout << "centerAHN2 Y: " << centerAHN2.getY() << std::endl;
        std::cout << "centerAHN3 X: " << centerAHN3.getX() << std::endl;
        std::cout << "centerAHN3 Y: " << centerAHN3.getY() << std::endl;
      }
    }
*/
		delete distance;
	}

	delete reporter;
	return Success;
}

bool reporter(CloudTools::DEM::SweepLineTransformation<float> *transformation, CloudTools::IO::Reporter *reporter)
{
	transformation->progress = [&reporter](float complete, const std::string &message)
	{
		reporter->report(complete, message);
		return true;
	};
	return false;
}

// Generate canopy height model of given DSM and DTM.
Difference<float>* generateCanopyHeightModel(const std::string& DTMinput, const std::string& DSMinput, const std::string& outpath,
                               CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  Difference<float> *comparison = new Difference<float>({ DTMinput, DSMinput }, outpath);
  if (!vm.count("quiet"))
  {
    comparison->progress = [&reporter](float complete, const std::string &message)
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
int countLocalMaximums(Difference<float> *chm, CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  SweepLineCalculation<float> *countLocalMax = new SweepLineCalculation<float>(
    { chm->target() }, 1, nullptr);

  int counter = 0;
  countLocalMax->computation = [&countLocalMax, &counter](int x, int y, const std::vector<Window<float>> &sources)
  {
    const Window<float> &source = sources[0];
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
    countLocalMax->progress = [&reporter](float complete, const std::string &message)
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
MatrixTransformation* antialias(Difference<float> *target, const std::string& outpath, CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  MatrixTransformation *filter = new MatrixTransformation(target->target(), outpath, 1);
  if (!vm.count("quiet"))
  {
    filter->progress = [&reporter](float complete, const std::string &message)
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
SweepLineTransformation<float>* eliminateNonTrees(MatrixTransformation* target, double threshold, CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  SweepLineTransformation<float> *eliminateNonTrees = new SweepLineTransformation<float>(
    { target->target() }, "ahn2_nosmall.tif", 0, nullptr);

  eliminateNonTrees->computation = [&eliminateNonTrees, &threshold](int x, int y, const std::vector<Window<float>> &sources)
  {
    const Window<float> &source = sources[0];
    if (!source.hasData() || source.data() < threshold)
      return static_cast<float>(eliminateNonTrees->nodataValue);
    else
      return source.data();
  };

  if (!vm.count("quiet"))
  {
    eliminateNonTrees->progress = [&reporter](float complete, const std::string &message)
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
std::vector<OGRPoint> collectSeedPoints(SweepLineTransformation<float>* target, CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  SweepLineCalculation<float> *collectSeeds = new SweepLineCalculation<float>(
    { target->target() }, 4, nullptr);

  std::vector<OGRPoint> seedPoints;
  collectSeeds->computation = [&collectSeeds, &seedPoints](int x, int y, const std::vector<Window<float>> &sources)
  {
    const Window<float> &source = sources[0];
    if (!source.hasData())
      return;

    for (int i = -collectSeeds->range(); i <= collectSeeds->range(); i++)
      for (int j = -collectSeeds->range(); j <= collectSeeds->range(); j++)
        if (source.data(i, j) > source.data(0, 0))
          return;
    seedPoints.emplace_back(x, y);
  };

  if (!vm.count("quiet"))
  {
    collectSeeds->progress = [&reporter](float complete, const std::string &message)
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
                                             CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  TreeCrownSegmentation *crownSegmentation = new TreeCrownSegmentation(
    { target->target() }, seedPoints, nullptr);
  if (!vm.count("quiet"))
  {
    crownSegmentation->progress = [&reporter](float complete, const std::string &message)
    {
      reporter->report(complete, message);
      return true;
    };
  }
  reporter->reset();
  crownSegmentation->execute();
  return crownSegmentation;
}

// Perform morphological filtering (erosion or dilation) on a previously build cluster map.
MorphologyClusterFilter* morphologyFiltering(const MorphologyClusterFilter::Method method, ClusterMap& clusterMap,
                                             const std::string& outpath, int threshold)
{
  MorphologyClusterFilter *morphologyFilter = new MorphologyClusterFilter(
    clusterMap, outpath, method, nullptr);
  morphologyFilter->threshold = threshold;
  morphologyFilter->execute();
  return morphologyFilter;
}

// Calculate the Hausdorff-distance of two cluster maps.
HausdorffDistance* calculateHausdorffDistance(ClusterMap& ahn2ClusterMap, ClusterMap& ahn3ClusterMap)
{
  HausdorffDistance *distance = new HausdorffDistance(ahn2ClusterMap, ahn3ClusterMap);
  distance->execute();
  return distance;
}

// Perform the algorithm on a DSM and a DTM.
std::pair<MorphologyClusterFilter*, TreeCrownSegmentation*> createRefinedClusterMap(int ahnVersion, const std::string& DTMinput,
                                                  const std::string& DSMinput, CloudTools::IO::Reporter *reporter, po::variables_map& vm)
{
  std::string chmOut, antialiasOut;
  if(ahnVersion == 3)
  {
    chmOut = "CHM.tif";
    antialiasOut = "antialias.tif";
  }
  else
  {
    chmOut = "ahn2_CHM.tif";
    antialiasOut = "ahn2_antialias.tif";
  }

  Difference<float> *CHM = generateCanopyHeightModel(DTMinput, DSMinput, chmOut, reporter, vm);
  std::cout << "CHM generated." << std::endl;

  // This step is optional.
  int counter = countLocalMaximums(CHM, reporter, vm);
  std::cout << "Number of local maximums are: " << counter << std::endl;

  MatrixTransformation *filter = antialias(CHM, antialiasOut, reporter, vm);
  std::cout << "Matrix transformation performed." << std::endl;

  delete CHM;

  double treeHeightThreshold = 1.5;
  SweepLineTransformation<float>* onlyTrees = eliminateNonTrees(filter, treeHeightThreshold, reporter, vm);
  std::cout << "Too small values eliminated." << std::endl;

  delete filter;

  // Count & collect local maximum values
  std::vector<OGRPoint> seedPoints = collectSeedPoints(onlyTrees, reporter, vm);

  ClusterMap cluster;
  cluster.setSizeX(onlyTrees->targetMetadata().rasterSizeX());
  cluster.setSizeY(onlyTrees->targetMetadata().rasterSizeY());

  TreeCrownSegmentation* crownSegmentation = treeCrownSegmentation(onlyTrees, seedPoints, reporter, vm);
  std::cout << "Tree crown segmentation performed." << std::endl;
  cluster = crownSegmentation->clusterMap();

  delete onlyTrees;

  // Perform morphological erosion on the cluster map
  int erosionThreshold = 6;
  MorphologyClusterFilter *erosion = morphologyFiltering(MorphologyClusterFilter::Method::Erosion, cluster, std::string(), erosionThreshold);
  std::cout << "Morphological erosion performed." << std::endl;

  MorphologyClusterFilter *dilation = morphologyFiltering(MorphologyClusterFilter::Method::Dilation, erosion->clusterMap, std::string());
  std::cout << "Morphological dilation performed." << std::endl;

  delete erosion;

  return std::make_pair(dilation, crownSegmentation);
}

// Write the result of Hausdorff-distance calculation to geotiff file.
void writeClusterMapsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
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
                                       segmentation->targetMetadata().rasterSizeX(), segmentation->targetMetadata().rasterSizeY(), 1,
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

void writeFullClustersToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, TreeCrownSegmentation* segmentation,
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
                                       segmentation->targetMetadata().rasterSizeX(), segmentation->targetMetadata().rasterSizeY(), 1,
                                       gdalType<int>(), nullptr);
  if (target == nullptr)
    throw std::runtime_error("Target file creation failed.");

  target->SetGeoTransform(&segmentation->targetMetadata().geoTransform()[0]);

  GDALRasterBand* targetBand = target->GetRasterBand(1);
  targetBand->SetNoDataValue(-1);

  int commonId = 1;
  std::vector<OGRPoint> points;
  for (auto elem : distance->closest())
  {
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

    ++commonId;

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

  commonId = -1;
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