#ifndef CLOUDTOOLS_INTERPOLATENODATA_HPP
#define CLOUDTOOLS_INTERPOLATENODATA_HPP

#include "CloudTools.Common/Operation.h"
#include "CloudTools.DEM/SweepLineTransformation.hpp"


namespace AHN
{
namespace Vegetation
{
template <typename DataType = float>
class InterpolateNoData : public CloudTools::DEM::SweepLineTransformation<DataType>
{
public:
  float threshold = 0.5;
  /// <summary>
  /// Initializes a new instance of the class. Loads input metadata and defines calculation.
  /// </summary>
  /// <param name="sourcePaths">The source files of the difference comparison.</param>
  /// <param name="targetPath">The target file of the difference comparison.</param>
  /// <param name="progress">The callback method to report progress.</param>
  InterpolateNoData(const std::vector<std::string>& sourcePaths,
             const std::string& targetPath,
             CloudTools::Operation::ProgressType progress = nullptr)
    : CloudTools::DEM::SweepLineTransformation<DataType>(sourcePaths, targetPath, nullptr, progress)
  {
    initialize();
  }

  /// <summary>
  /// Initializes a new instance of the class. Loads input metadata and defines calculation.
  /// </summary>
  /// <param name="sourceDatasets">The source datasets of the difference comparison.</param>
  /// <param name="targetPath">The target file of the difference comparison.</param>
  /// <param name="progress">The callback method to report progress.</param>
  InterpolateNoData(const std::vector<GDALDataset*>& sourceDatasets,
                    const std::string& targetPath,
                    CloudTools::Operation::ProgressType progress = nullptr,
                    float ratio = 0.5)
    : CloudTools::DEM::SweepLineTransformation<DataType>(sourceDatasets, targetPath, 0, nullptr, progress)
  {
    initialize();
  }

  InterpolateNoData(const InterpolateNoData&) = delete;
  InterpolateNoData& operator=(const InterpolateNoData&) = delete;

private:
  void initialize();
};

<DataType>
void InterpolateNoData<DataType> initialize()
{
  this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
  {
    const Window<float>& source = sources[0];
    if (source.hasData())
      return source.data();

    int counter = 0;
    float data = 0;

    for (int i = -this->range(); i <= this->range(); i++)
      for (int j = -this->range(); j <= this->range(); j++)
        if (source.hasData())
        {
          counter++;
          data += source.data(i, j);
        }

    if (ratio <= 1.0 && ratio >= 0.0)
      this->threshold = ratio;

    if (counter < (std::pow((this->range() * 2 + 1), 2) - 1) * this->threshold)
      return static_cast<float>(this->nodataValue);

    return static_cast<float>(data / counter);
  };
}
}
}

#endif //CLOUDTOOLS_INTERPOLATENODATA_HPP
