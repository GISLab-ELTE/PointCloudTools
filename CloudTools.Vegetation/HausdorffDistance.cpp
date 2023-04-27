#include <random>

#include "HausdorffDistance.h"

namespace CloudTools
{
namespace Vegetation
{
void HausdorffDistance::onExecute()
{
	progress(0.f, "Performing Hausdorff-distance based cluster pairing.");

	randomizePoints();

	// A -> B
	for (const GUInt32 indexA : clusterMapA.clusterIndexes())
	{
		for (const GUInt32 indexB : clusterMapB.clusterIndexes())
		{
			auto centerB = clusterMapB.center2D(indexB);
			auto centerA = clusterMapA.center2D(indexA);
			double distance = centerA.Distance(&centerB);
			if (distance >= maximumDistance)
				continue;

			double cmax = 0;
			for (const OGRPoint& pointA : clusterMapA.points(indexA))
			{
				double cmin = std::numeric_limits<double>::max();
				for (const OGRPoint& pointB : clusterMapB.points(indexB))
				{
					double dist = pointA.Distance(&pointB);
					if (dist < cmax)
					{
						cmin = 0;
						break;
					}
					cmin = std::min(dist, cmin);
				}
				cmax = std::max(cmin, cmax);
			}
			hausdorffDistancesA.emplace(std::make_pair(std::make_pair(indexA, indexB), cmax));
		}
	}

	// TODO: add intermediate reporting into above loop
	if (progress)
		progress(0.35f, "Epoch-A to B distances calculated.");

	// B -> A
	for (const GUInt32 indexB : clusterMapB.clusterIndexes())
	{
		for (const GUInt32 indexA : clusterMapA.clusterIndexes())
		{
			auto centerB = clusterMapB.center2D(indexB);
			auto centerA = clusterMapA.center2D(indexA);
			double distance = centerA.Distance(&centerB);
			if (distance >= maximumDistance)
				continue;

			double cmax = 0;
			for (const OGRPoint& pointB : clusterMapB.points(indexB))
			{
				double cmin = std::numeric_limits<double>::max();
				for (const OGRPoint& pointA : clusterMapA.points(indexA))
				{
					double dist = pointB.Distance(&pointA);
					if (dist < cmax)
					{
						cmin = 0;
						break;
					}
					cmin = std::min(dist, cmin);
				}
				cmax = std::max(cmin, cmax);
			}
			hausdorffDistancesB.emplace(std::make_pair(std::make_pair(indexB, indexA), cmax));
		}
	}

	// TODO: add intermediate reporting into above loop
	if (progress)
		progress(0.7f, "Epoch-B to A distances calculated.");

	bool hasConflict;
	do
	{
		hasConflict = false;
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
					auto resA = hausdorffDistancesA.find({indexA, indexB});
					auto resB = hausdorffDistancesB.find({indexB, indexA});

					if (resA != hausdorffDistancesA.end()
					    && resB != hausdorffDistancesB.end())
					{
						double newDist = std::max(resA->second, resB->second);

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
				hasConflict = true;
			}
			closestClusters.insert(std::make_pair(std::make_pair(minPair.second.first, minPair.first),
			                                      minPair.second.second));
		}
	}
	while (hasConflict);

	if (progress)
		progress(0.8f, "Cluster map pairs calculated.");

	for (GUInt32 indexA : clusterMapA.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&indexA](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.first == indexA;
		                 }) == closestClusters.end())
			lonelyClustersA.push_back(indexA);

	if (progress)
		progress(0.9f, "Lonely Epoch-A clusters calculated.");

	for (GUInt32 indexB : clusterMapB.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&indexB](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.second == indexB;
		                 }) == closestClusters.end())
			lonelyClustersB.push_back(indexB);

	if (progress)
		progress(1.f, "Lonely Epoch-B clusters calculated.");
}

GUInt32 HausdorffDistance::closestCluster(GUInt32 index)
{
	GUInt32 closest = index;
	std::map<double, GUInt32> distances;
	for (auto elem : hausdorffDistancesA)
		if (elem.first.first == index)
			distances.emplace(std::make_pair(elem.second, elem.first.second));

	closest = distances.begin()->second;
	return closest;
}

std::map<std::pair<GUInt32, GUInt32>, double> HausdorffDistance::distances() const
{
	return hausdorffDistancesA;
}


void HausdorffDistance::randomizePoints()
{
	clusterMapA.shuffle();
	clusterMapB.shuffle();
}
} // Vegetation
} // CloudTools