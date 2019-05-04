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
					OGRPoint point = AHN3ClusterMap.center(otherIndex);
					double newDist = AHN2ClusterMap.center(index).Distance(&point);
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

	for (GUInt32 index : AHN2ClusterMap.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.first == index;
		                 }) == closestClusters.end())
			lonelyClustersAHN2.push_back(index);

	for (GUInt32 index : AHN3ClusterMap.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.second == index;
		                 }) == closestClusters.end())
			lonelyClustersAHN3.push_back(index);
}

const std::map<std::pair<GUInt32, GUInt32>, double>& CentroidDistance::closest() const
{
	return closestClusters;
}

const std::vector<GUInt32>& CentroidDistance::lonelyAHN2() const
{
	return lonelyClustersAHN2;
}

const std::vector<GUInt32>& CentroidDistance::lonelyAHN3() const
{
	return lonelyClustersAHN3;
}
}
}
