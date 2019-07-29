#include "Process.h"

#include <numeric>

#include <CloudTools.DEM/SweepLineCalculation.hpp>

#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>
#include "EliminateNonTrees.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.h"

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

void writeClusterMapToFile(const ClusterMap& cluster,
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

void Process::createRefinedClusterMap(int ahnVersion, const std::string& DTMinput, const std::string& DSMinput,
                                      const std::string& outputDir, RasterMetadata& targetMetadata,
                                      CloudTools::IO::Reporter* reporter, po::variables_map& vm)
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

  Difference<float> comparison({DTMinput, DSMinput}, chmOut);
  if (!vm.count("quiet"))
  {
    runReporter(&comparison);
  }
  comparison.execute();
  reporter->reset();

  InterpolateNoData interpolation({comparison.target()}, interpolOut);
  if (!vm.count("quiet"))
  {
    runReporter(&interpolation);
  }
  interpolation.execute();
  reporter->reset();

  MatrixTransformation filter(interpolation.target(), antialiasOut, 1);
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

  EliminateNonTrees elimination({filter.target()}, nosmallOut);
  if (!vm.count("quiet"))
  {
	  runReporter(&elimination);
  }
  elimination.execute();
  reporter->reset();

  std::vector<OGRPoint> seedPoints = collectSeedPoints(elimination.target(), vm);

  ClusterMap cluster;
  TreeCrownSegmentation segmentation({ elimination.target() }, seedPoints);
  if (!vm.count("quiet"))
  {
	  runReporter(&segmentation);
  }
  segmentation.execute();
  reporter->reset();

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
}
}
}