#ifndef CLOUDTOOLS_VOLUMEDIFFERENCE_H
#define CLOUDTOOLS_VOLUMEDIFFERENCE_H

#include <map>

#include <CloudTools.DEM/ClusterMap.h>
#include "DistanceCalculation.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
class VolumeDifference
{
public:
  enum AHN
  {
    AHN2, AHN3
  };

  ClusterMap ahn2, ahn3;
  std::shared_ptr<DistanceCalculation> distance;

  double ahn2_fullVolume, ahn3_fullVolume;
  std::map<GUInt32, double> ahn2_lonelyVolume, ahn3_lonelyVolume;
  std::map<std::pair<GUInt32, GUInt32>, double> diffs;

  VolumeDifference(ClusterMap& ahn2_map,
                   ClusterMap& ahn3_map,
                   std::shared_ptr<DistanceCalculation> distance)
    : ahn2(ahn2_map), ahn3(ahn3_map), distance(distance)
  {
    calculateVolume();
  }

private:
  void calculateVolume();

  std::pair<double, std::map<GUInt32, double>> calculateLonelyEpochVolume(AHN epoch, ClusterMap &map);

  void calculateDifference();
};
}
}

#endif //CLOUDTOOLS_VOLUMEDIFFERENCE_H
