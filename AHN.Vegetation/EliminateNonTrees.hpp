#ifndef CLOUDTOOLS_ELIMINATENONTREES_HPP
#define CLOUDTOOLS_ELIMINATENONTREES_HPP

#include "CloudTools.Common/Operation.h"
#include "CloudTools.DEM/SweepLineTransformation.hpp"

namespace AHN
{
namespace Vegetation
{
class EliminateNonTrees : public CloudTools::DEM::SweepLineTransformation<float>
{
public:
  float threshold;
  /// <summary>
  /// Initializes a new instance of the class. Loads input metadata and defines calculation.
  /// </summary>
  /// <param name="sourcePaths">The source files of the difference comparison.</param>
  /// <param name="targetPath">The target file of the difference comparison.</param>
  /// <param name="progress">The callback method to report progress.</param>
  EliminateNonTrees(const std::vector<std::string>& sourcePaths,
                    const std::string& targetPath,
                    CloudTools::Operation::ProgressType progress = nullptr,
                    float threshold = 1.5)
    : CloudTools::DEM::SweepLineTransformation<float>(sourcePaths, targetPath, nullptr, progress),
      threshold(threshold)
  {
    initialize();
  }

  /// <summary>
  /// Initializes a new instance of the class. Loads input metadata and defines calculation.
  /// </summary>
  /// <param name="sourceDatasets">The source datasets of the difference comparison.</param>
  /// <param name="targetPath">The target file of the difference comparison.</param>
  /// <param name="progress">The callback method to report progress.</param>
  EliminateNonTrees(const std::vector<GDALDataset*>& sourceDatasets,
                    const std::string& targetPath,
                    CloudTools::Operation::ProgressType progress = nullptr,
                    float threshold = 1.5)
    : CloudTools::DEM::SweepLineTransformation<float>(sourceDatasets, targetPath, 0, nullptr, progress),
      threshold(threshold)
  {
    initialize();
  }

  EliminateNonTrees(const EliminateNonTrees&) = delete;
  EliminateNonTrees& operator=(const EliminateNonTrees&) = delete;

private:
  void initialize();
};

void EliminateNonTrees::initialize()
{
  this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
  {
    const Window<float>& source = sources[0];

    if (!source.hasData() || source.data() < this->threshold)
      return static_cast<float>(this->nodataValue);
    else
      return source.data();
  };
}
}
}