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
  Process(CloudTools::IO::Reporter* reporter);

  void run();

private:
  CloudTools::IO::Reporter* reporter;
  RasterMetadata targetMetadata;

  double treeHeightThreshold;
  float interpolationRatio;

  void createRefinedClusterMap(int ahnVersion, const std::string& DTMinput, const std::string& DSMinput,
                               const std::string& outputDir, RasterMetadata& targetMetadata,
                               CloudTools::IO::Reporter* reporter, po::variables_map& vm);

  bool runReporter(CloudTools::DEM::Calculation* operation);

  std::vector<OGRPoint> collectSeedPoints(GDALDataset* target, po::variables_map& vm);

  void writeClusterMapToFile(const ClusterMap& cluster,
	  const RasterMetadata& metadata, const std::string& outpath);
};
}
}
