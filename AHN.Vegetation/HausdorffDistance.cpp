#include "HausdorffDistance.h"

namespace AHN
{
namespace Vegetation
{
void HausdorffDistance::onExecute()
{
	progress(0.f, "Performig Hausdorff-distance based cluster pairing.");
	
	std::vector<double> distances;
	double minDistance;

	for (GUInt32 index : AHN2ClusterMap.clusterIndexes())
	{
		for (GUInt32 otherIndex : AHN3ClusterMap.clusterIndexes())
		{
			auto ahn3Center = AHN3ClusterMap.center2D(otherIndex);
			auto ahn2Center = AHN2ClusterMap.center2D(index);
			double distance = ahn2Center.Distance(&ahn3Center);
			if (distance >= maximumDistance)
				continue;

			distances.clear();
			for (const OGRPoint& point : AHN2ClusterMap.points(index))
			{
				std::vector<OGRPoint> ahn3Points = AHN3ClusterMap.points(otherIndex);
				minDistance = point.Distance(&ahn3Points[0]);
				for (const OGRPoint& otherPoint : ahn3Points)
				{
					double dist = point.Distance(&otherPoint);
					if (dist < minDistance)
					{
						minDistance = dist;
					}
				}
				distances.push_back(minDistance);
			}
			ahn2HausdorffDistances.emplace(std::make_pair(std::make_pair(index, otherIndex),
			                                              *std::max_element(distances.begin(), distances.end())));
		}
	}

	// TODO: add intermediate reporting into above loop
	if (progress)
		progress(0.35f, "AHN2 to AHN3 distances calculated.");

	for (GUInt32 index : AHN3ClusterMap.clusterIndexes())
	{
		for (GUInt32 otherIndex : AHN2ClusterMap.clusterIndexes())
		{
			auto ahn2Center = AHN2ClusterMap.center2D(otherIndex);
			auto ahn3Center = AHN3ClusterMap.center2D(index);
			double distance = ahn2Center.Distance(&ahn3Center);
			if (distance >= maximumDistance)
				continue;

			distances.clear();
			for (const OGRPoint& point : AHN3ClusterMap.points(index))
			{
				std::vector<OGRPoint> ahn3Points = AHN2ClusterMap.points(otherIndex);
				minDistance = point.Distance(&ahn3Points[0]);
				for (const OGRPoint& otherPoint : ahn3Points)
				{
					double dist = point.Distance(&otherPoint);
					if (dist < minDistance)
					{
						minDistance = dist;
					}
				}
				distances.push_back(minDistance);
			}
			ahn3HausdorffDistances.emplace(std::make_pair(std::make_pair(otherIndex, index),
			                                              *std::max_element(distances.begin(), distances.end())));
		}
	}
	
	// TODO: add intermediate reporting into above loop
	if (progress)
		progress(0.7f, "AHN3 to AHN2 distances calculated.");

  bool hasConflict;
  do
  {
    hasConflict = false;
    std::multimap<GUInt32, std::pair<GUInt32, double>> ahn3Key;

    for (const GUInt32 index : AHN2ClusterMap.clusterIndexes())
    {
      double dist = std::numeric_limits<double>::max();
      GUInt32 i = -1;

      if (std::find_if(closestClusters.begin(),
                       closestClusters.end(),
                       [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
                       {
                         return p.first.first == index;
                       })
          == closestClusters.end())
      {
        for (const GUInt32 otherIndex : AHN3ClusterMap.clusterIndexes())
        {
          auto ahn2Res = ahn2HausdorffDistances.find({index, otherIndex});
          auto ahn3Res = ahn3HausdorffDistances.find({index, otherIndex});

          if (ahn2Res != ahn2HausdorffDistances.end()
            && ahn3Res != ahn3HausdorffDistances.end())
          {
            double newDist = std::max(ahn2Res->second, ahn3Res->second);

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
        }

        if (dist <= maximumDistance)
        {
          ahn3Key.insert(std::make_pair(i, std::make_pair(index, dist))); // AHN3 index is key
        }
      }
    }

    for (auto it = ahn3Key.begin(), end = ahn3Key.end();
         it != end; it = ahn3Key.upper_bound(it->first))
    {
      GUInt32 ahn3 = it->first;
      std::pair<GUInt32, std::pair<GUInt32, double>> minPair = *it;
      if (ahn3Key.count(ahn3) > 1)
      {
        auto iter = it;
        while (iter != end && iter->first == it->first)
        {
          if (minPair.second.second > iter->second.second)
            minPair = *iter;
          iter++;
        }
        hasConflict = true;
      }
      closestClusters.insert(std::make_pair(std::make_pair(minPair.second.first, minPair.first),
                                            minPair.second.second));
    }
  }
  while (hasConflict);

	if (progress)
		progress(0.8f, "Cluster map pairs calculated.");

  for (GUInt32 index : AHN2ClusterMap.clusterIndexes())
    if (std::find_if(closestClusters.begin(), closestClusters.end(),
                     [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
                     {
                       return item.first.first == index;
                     }) == closestClusters.end())
      lonelyClustersAHN2.push_back(index);

  if (progress)
    progress(0.9f, "Lonely AHN2 clusters calculated.");

  for (GUInt32 index : AHN3ClusterMap.clusterIndexes())
    if (std::find_if(closestClusters.begin(), closestClusters.end(),
                     [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
                     {
                       return item.first.second == index;
                     }) == closestClusters.end())
      lonelyClustersAHN3.push_back(index);

	if (progress)
		progress(1.f, "Lonely AHN3 clusters calculated.");
}

GUInt32 HausdorffDistance::closestCluster(GUInt32 index)
{
	GUInt32 closest = index;
	std::map<double, GUInt32> distances;
	for (auto elem : ahn2HausdorffDistances)
		if (elem.first.first == index)
			distances.emplace(std::make_pair(elem.second, elem.first.second));

	closest = distances.begin()->second;
	return closest;
}

std::map<std::pair<GUInt32, GUInt32>, double> HausdorffDistance::distances() const
{
	return ahn2HausdorffDistances;
}
} // Vegetation
} // AHN
