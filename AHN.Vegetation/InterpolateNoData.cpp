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
          std::cout << "counter: " << counter << std::endl;
        }

    if (this->threshold > 1.0 || this->threshold < 0.0)
      this->threshold = 0.5;

    std::cout << data << ", " << counter << ", " << this->threshold << std::endl;
    if (counter < ((std::pow((this->range() * 2 + 1), 2.0) - 1) * this->threshold))
    {
      std::cout << "Returning nodata-" << std::endl;
      return static_cast<float>(this->nodataValue);
    }

    std::cout << "Returning " << (data / counter) << std::endl;
    return static_cast<float>(data / counter);
  };
}
}
}
