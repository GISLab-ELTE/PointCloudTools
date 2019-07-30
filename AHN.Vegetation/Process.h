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
  Process(int AHNVersion, const std::string& DTMInputPath, const std::string& DSMInputPath,
          const std::string& outputDir, CloudTools::IO::Reporter* reporter, po::variables_map& vm)
          : AHNVersion(AHNVersion), DTMInputPath(DTMInputPath), DSMInputPath(DSMInputPath),
          outputDir(outputDir), reporter(reporter), vm(vm)
  {

  }

  void setAHNVersion(int);

  ClusterMap run(int);

  ClusterMap map();

private:
  int AHNVersion;
  std::string DTMInputPath, DSMInputPath, outputDir;
  po::variables_map& vm;
  CloudTools::IO::Reporter* reporter;
  RasterMetadata targetMetadata;

  double treeHeightThreshold;
  float interpolationRatio;

  ClusterMap cluster;

  void createRefinedClusterMap();

  bool runReporter(CloudTools::DEM::Calculation* operation);

  std::vector<OGRPoint> collectSeedPoints(GDALDataset* target, po::variables_map& vm);

  void writeClusterMapToFile(const ClusterMap& cluster,
	  const RasterMetadata& metadata, const std::string& outpath);
};
}
}
