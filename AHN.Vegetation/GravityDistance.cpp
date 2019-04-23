#include "GravityDistance.h"
#include <algorithm>
#include <iostream>

namespace AHN
{
namespace Vegetation
{
void GravityDistance::onExecute()
{
  bool hasChanged;
  do
  {
    hasChanged = false;
    std::multimap<GUInt32, std::pair<GUInt32, double>> ahn3Key;
    //std::cout << "step 1" << std::endl;
    for (const GUInt32 index : AHN2clusterMap.clusterIndexes())
    {
      double dist = 1000;
      GUInt32 i = -1;
      //if (std::find(AHN2clusterMap.clusterIndexes().begin(),
      //              AHN2clusterMap.clusterIndexes().end(), index) == AHN2clusterMap.clusterIndexes().end())
      if (std::find_if(closestClusters.begin(),
                       closestClusters.end(),
                       [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
                       {
                          return p.first.first == index;
                       })
          == closestClusters.end())
      {
        for (const GUInt32 otherIndex : AHN3clusterMap.clusterIndexes())
        {
          OGRPoint p = AHN3clusterMap.center(otherIndex);
          double newDist = AHN2clusterMap.center(index).Distance(&p);
          if (newDist < dist &&
              std::find_if(closestClusters.begin(),
               closestClusters.end(),
               [&otherIndex](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
               {
                 return p.first.second == otherIndex;
               })
               == closestClusters.end())
          {
            dist = newDist;
            i = otherIndex;
          }
        }

        if (dist <= maximumDistance)
        {
          ahn3Key.insert(std::make_pair(i, std::make_pair(index, dist))); // AHN3 index is key
        }
      }
    }
    //std::cout << "step 2" << std::endl;
    //std::cout << "ahn3Key size: " << ahn3Key.size() << std::endl;

    for (std::multimap<GUInt32, std::pair<GUInt32, double>>::iterator it = ahn3Key.begin(), end = ahn3Key.end();
      it != end; it = ahn3Key.upper_bound(it->first))
    //for (auto it = ahn3Key.begin(); it != ahn3Key.end(); it = ahn3Key.upper_bound(it->first))
    {
      //std::cout << "current key: " << it->first << std::endl;
      GUInt32 ahn3 = it->first;
      std::pair<GUInt32, std::pair<GUInt32, double>> minPair = *it;
      if (ahn3Key.count(ahn3) > 1)
      {
        auto iter = it;
        while (iter->first == it->first)
        {
          if (minPair.second.second > iter->second.second)
            minPair = *iter;
          iter++;
        }
        hasChanged = true;
      }
      closestClusters.insert(std::make_pair(std::make_pair(minPair.second.first, minPair.first), minPair.second.second));
    }
    //std::cout << "step 3" << std::endl;
    //std::cout << "closestClusters size: " << closestClusters.size() << std::endl;
  }
  while(hasChanged);

  for(GUInt32 index : AHN2clusterMap.clusterIndexes())
    if(std::find_if(closestClusters.begin(), closestClusters.end(),
                    [&index](const std::pair<std::pair<GUInt32, GUInt32>, double> &item)
                    { return item.first.first == index; }) == closestClusters.end())
      lonelyClustersAHN2.push_back(index);

  //std::cout << "step 4" << std::endl;

  for(GUInt32 index : AHN3clusterMap.clusterIndexes())
    if(std::find_if(closestClusters.begin(), closestClusters.end(),
                    [&index](const std::pair<std::pair<GUInt32, GUInt32>, double> &item)
                    { return item.first.second == index; }) == closestClusters.end())
      lonelyClustersAHN3.push_back(index);
}

const std::map<std::pair<GUInt32, GUInt32>, double>& GravityDistance::closest()
{
  return closestClusters;
}

const std::vector<GUInt32>& GravityDistance::lonelyAHN2()
{
  return lonelyClustersAHN2;
}

const std::vector<GUInt32>& GravityDistance::lonelyAHN3()
{
  return lonelyClustersAHN3;
}
}
}
