#include "Process.h"

#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/Algorithms/MatrixTransformation.h>

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


}
}
}