#include "GravityDistance.h"
#include <algorithm>

namespace AHN
{
namespace Vegetation
{
void GravityDistance::onExecute()
{
  std::vector<OGRPoint> ahn2Centers, ahn3Centers;
  for (GUInt32 index : AHN2clusterMap.clusterIndexes())
    ahn2Centers.push_back(AHN2clusterMap.center(index));

  for (GUInt32 index : AHN3clusterMap.clusterIndexes())
    ahn3Centers.push_back(AHN3clusterMap.center(index));

  bool hasChanged;
  do
  {
    hasChanged = false;
    for (GUInt32 index : AHN2clusterMap.clusterIndexes())
    {
      double dist = 0;
      GUInt32 i = -1;
      if (std::find(AHN2clusterMap.clusterIndexes().begin(),
        AHN2clusterMap.clusterIndexes().end(), index) == AHN2clusterMap.clusterIndexes().end())
      {
        for (GUInt32 otherIndex : AHN3clusterMap.clusterIndexes())
        {
          OGRPoint p = AHN3clusterMap.center(otherIndex);
          dist = std::min(dist, AHN2clusterMap.center(index).Distance(&p));
          i = otherIndex;
        }
      }

      if (dist <= maximumDistance)
        closestClusters.insert(std::make_pair(std::make_pair(index, i), dist));

      for (auto& pair : closestClusters)
      {
        // TODO: loop through ahn3 indexes --> if an index occurs multiple times, keep the one with minimum distance, throw away the others --> hasChanged
      }
    }
  }
  while(hasChanged);
}
}
}
