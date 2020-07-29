#pragma once

#include <boost/program_options.hpp>

#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/Calculation.h>
#include <CloudTools.DEM/ClusterMap.h>

#include "InterpolateNoData.h"
#include "DistanceCalculation.h"

#include <gdal_priv.h>

namespace po = boost::program_options;

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
class Process
{
public:
  enum DifferenceMethod
  {
    Hausdorff,
    Centroid
  };

  Process(int AHNVersion, const std::string& AHN2DTMInputPath, const std::string& AHN2DSMInputPath,
          const std::string& AHN3DTMInputPath, const std::string& AHN3DSMInputPath,
          const std::string& outputDir, CloudTools::IO::Reporter* reporter, po::variables_map& vm,
          DifferenceMethod method = DifferenceMethod::Centroid)
          : AHNVersion(AHNVersion), AHN2DTMInputPath(AHN2DTMInputPath), AHN2DSMInputPath(AHN2DSMInputPath),
            AHN3DTMInputPath(AHN3DTMInputPath), AHN3DSMInputPath(AHN3DSMInputPath),
            outputDir(outputDir), reporter(reporter), vm(vm), method(method)
  {

  }

  void run();

private:
  int AHNVersion;
  DifferenceMethod method;
  std::string AHN2DTMInputPath, AHN2DSMInputPath, AHN3DTMInputPath, AHN3DSMInputPath, outputDir;
  po::variables_map& vm;
  CloudTools::IO::Reporter* reporter;
  RasterMetadata targetMetadata;

  double treeHeightThreshold;
  float interpolationRatio;

  ClusterMap clusterAHN2, clusterAHN3;

  ClusterMap preprocess(int version);

  void process();

  bool runReporter(CloudTools::DEM::Calculation* operation);

  std::vector<OGRPoint> collectSeedPoints(GDALDataset* target, po::variables_map& vm);

  void writeClusterMapToFile(const ClusterMap& cluster,
	  const RasterMetadata& metadata, const std::string& outpath);

  void writeClusterPairsToFile(ClusterMap& ahn2Map, ClusterMap& ahn3Map, RasterMetadata& targetMetadata,
                               const std::string& ahn2Outpath, DistanceCalculation* distance);
};
}
}
