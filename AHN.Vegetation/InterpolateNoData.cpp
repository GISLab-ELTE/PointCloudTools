#include "InterpolateNoData.h"

namespace AHN
{
namespace Vegetation
{
void InterpolateNoData::initialize()
{
  this->computation = [this](int x, int y, const std::vector<CloudTools::DEM::Window<float>>& sources)
  {
    const CloudTools::DEM::Window<float>& source = sources[0];
    if (source.hasData())
      return source.data();

    int counter = 0;
    float data = 0;

    for (int i = -this->range(); i <= this->range(); i++)
      for (int j = -this->range(); j <= this->range(); j++)
        if (source.hasData(i, j))
        {
          counter++;
          data += source.data(i, j);
        }

    if (this->threshold > 1.0 || this->threshold < 0.0)
      this->threshold = 0.5;

    if (counter < ((std::pow((this->range() * 2 + 1), 2.0) - 1) * this->threshold))
    {
      return static_cast<float>(this->nodataValue);
    }

    return static_cast<float>(data / counter);
  };
}
}
}
