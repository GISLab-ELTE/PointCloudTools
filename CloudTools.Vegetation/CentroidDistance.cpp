#include <algorithm>
#include <limits>

#include "CentroidDistance.h"

namespace CloudTools
{
namespace Vegetation
{
void CentroidDistance::onExecute()
{
	progress(0.f, "Performig centroid distance based cluster pairing.");
	
	bool hasChanged;
	do
	{
		hasChanged = false;
		std::multimap<GUInt32, std::pair<GUInt32, double>> bKey;

		for (const GUInt32 indexA : clusterMapA.clusterIndexes())
		{
			double dist = std::numeric_limits<double>::max();
			GUInt32 i = -1;

			if (std::find_if(closestClusters.begin(),
			                 closestClusters.end(),
			                 [&indexA](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
			                 {
				                 return p.first.first == indexA;
			                 })
			    == closestClusters.end())
			{
				for (const GUInt32 indexB : clusterMapB.clusterIndexes())
				{
					OGRPoint centerA = clusterMapA.center2D(indexA);
					OGRPoint centerB = clusterMapB.center2D(indexB);
					double newDist = centerA.Distance(&centerB);
					if (newDist < dist &&
					    std::find_if(closestClusters.begin(),
					                 closestClusters.end(),
					                 [&indexB](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
					                 {
						                 return p.first.second == indexB;
					                 })
					    == closestClusters.end())
					{
						dist = newDist;
						i = indexB;
					}
				}

				if (dist <= maximumDistance)
				{
					bKey.insert(std::make_pair(i, std::make_pair(indexA, dist))); // clusterMapB index is key
				}
			}
		}

		for (auto it = bKey.begin(), end = bKey.end();
		     it != end; it = bKey.upper_bound(it->first))
		{
			GUInt32 indexB = it->first;
			std::pair<GUInt32, std::pair<GUInt32, double>> minPair = *it;
			if (bKey.count(indexB) > 1)
			{
				auto iter = it;
				while (iter != end && iter->first == it->first)
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

	for (GUInt32 index : clusterMapA.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.first == index;
		                 }) == closestClusters.end())
			lonelyClustersA.push_back(index);

	if (progress)
		progress(0.9f, "Lonely A clusters calculated.");

	for (GUInt32 index : clusterMapB.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.second == index;
		                 }) == closestClusters.end())
			lonelyClustersB.push_back(index);

	if (progress)
		progress(1.f, "Lonely B clusters calculated.");
}
} // Vegetation
} // CloudTools
