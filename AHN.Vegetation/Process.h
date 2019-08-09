#pragma once

#include <boost/program_options.hpp>

#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/Calculation.h>
#include <CloudTools.DEM/ClusterMap.h>

#include "InterpolateNoData.h"

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

  Process(int AHNVersion, const std::string& DTMInputPath, const std::string& DSMInputPath,
          const std::string& outputDir, CloudTools::IO::Reporter* reporter, po::variables_map& vm,
          DifferenceMethod method = DifferenceMethod::Centroid)
          : AHNVersion(AHNVersion), DTMInputPath(DTMInputPath), DSMInputPath(DSMInputPath),
          outputDir(outputDir), reporter(reporter), vm(vm), method(method)
  {

  }

  void setAHNVersion(int);

  void run(int);

  ClusterMap map();

private:
  int AHNVersion;
  DifferenceMethod method;
  std::string DTMInputPath, DSMInputPath, outputDir;
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
};
}
}
