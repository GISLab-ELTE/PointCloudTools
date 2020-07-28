#include "Process.h"

#include <numeric>

#include <CloudTools.DEM/SweepLineCalculation.hpp>

#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>
#include "EliminateNonTrees.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"
#include "HausdorffDistance.h"
#include "CentroidDistance.h"

using namespace CloudTools::IO;
using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
void Process::setAHNVersion(int version)
{
	this->AHNVersion = version;
}

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
																			const std::string& ahn2Outpath, DistanceCalculation* distance)
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

  Difference<float> comparison({dtm, dsm}, chmOut);
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

  EliminateNonTrees elimination({filter.target()}, nosmallOut);
  if (!vm.count("quiet"))
  {
	  runReporter(&elimination);
  }
  elimination.execute();
  reporter->reset();
  std::cout << "Small points eliminated.." << std::endl;

  InterpolateNoData interpolation({elimination.target()}, interpolOut);
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
  std::unique_ptr<DistanceCalculation> distance;
	if (method == Hausdorff)
	{
		std::cout << "Using Hausdorff distance to pair up clusters." << std::endl;
		distance.reset(new HausdorffDistance(clusterAHN2, clusterAHN3));
		std::cout << "Hausdorff distance calculated." << std::endl;

		/*pairs = distance->closest().size();
		lonelyAHN2 = distance->lonelyAHN2().size();
		lonelyAHN3 = distance->lonelyAHN3().size();*/
	}

	if (method == Centroid)
	{
		std::cout << "Using gravity distance to pair up clusters." << std::endl;
    distance.reset(new CentroidDistance(clusterAHN2, clusterAHN3));
		std::cout << "Gravity distance calculated." << std::endl;
	}

	distance->execute();

	writeClusterPairsToFile(clusterAHN2, clusterAHN3, targetMetadata, "cluster_pairs.tif", distance.get());

  std::cout << "Total number of clusters in AHN2: " << clusterAHN2.clusterIndexes().size() << std::
  endl;
  std::cout << "Total number of clusters in AHN3: " << clusterAHN3.clusterIndexes().size() << std::
  endl;
  std::cout << "Pairs found: " << distance->closest().size() << std::endl;
  std::cout << "Number of unpaired clusters in AHN2: " << distance->lonelyAHN2().size() << std::endl;
  std::cout << "Number of unpaired clusters in AHN3: " << distance->lonelyAHN3().size() << std::endl;
}

ClusterMap Process::map()
{
	return ClusterMap();
	//return cluster;
}

void Process::run()
{
	//setAHNVersion(version);

	clusterAHN2 = preprocess(2);
	clusterAHN3 = preprocess(3);

  process();
}
}
}