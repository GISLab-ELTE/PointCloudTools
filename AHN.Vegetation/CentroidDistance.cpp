#include <algorithm>
#include <limits>

#include "CentroidDistance.h"

namespace AHN
{
namespace Vegetation
{
void CentroidDistance::onExecute()
{
	bool hasChanged;
	do
	{
		hasChanged = false;
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
					OGRPoint ahn2Center = AHN2ClusterMap.center2D(index);
					OGRPoint ahn3Center = AHN3ClusterMap.center2D(otherIndex);
					double newDist = ahn2Center.Distance(&ahn3Center);
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

		for (auto it = ahn3Key.begin(), end = ahn3Key.end();
		     it != end; it = ahn3Key.upper_bound(it->first))
		{
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
			closestClusters.insert(std::make_pair(std::make_pair(minPair.second.first, minPair.first),
			                                      minPair.second.second));
		}
	}
	while (hasChanged);

	// TODO: add intermediate reporting into above loop
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
} // Vegetation
} // AHN
