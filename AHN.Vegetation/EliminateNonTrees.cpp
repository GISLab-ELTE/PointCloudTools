#include "EliminateNonTrees.h"

namespace AHN
{
namespace Vegetation
{
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
