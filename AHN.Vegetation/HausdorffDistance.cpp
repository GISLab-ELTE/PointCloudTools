#include <random>

#include "HausdorffDistance.h"

namespace AHN
{
namespace Vegetation
{
void HausdorffDistance::onExecute()
{
	progress(0.f, "Performig Hausdorff-distance based cluster pairing.");

	randomizePoints();

	// AHN2 -> AHN3
	for (const GUInt32 ahn2Index : AHN2ClusterMap.clusterIndexes())
	{
		for (const GUInt32 ahn3Index : AHN3ClusterMap.clusterIndexes())
		{
			auto ahn3Center = AHN3ClusterMap.center2D(ahn3Index);
			auto ahn2Center = AHN2ClusterMap.center2D(ahn2Index);
			double distance = ahn2Center.Distance(&ahn3Center);
			if (distance >= maximumDistance)
				continue;

			double cmax = 0;
			for (const OGRPoint& ahn2Point : AHN2ClusterMap.points(ahn2Index))
			{
				double cmin = std::numeric_limits<double>::max();
				for (const OGRPoint& ahn3Point : AHN3ClusterMap.points(ahn3Index))
				{
					double dist = ahn2Point.Distance(&ahn3Point);
					if (dist < cmax)
					{
						cmin = 0;
						break;
					}
					cmin = std::min(dist, cmin);
				}
				cmax = std::max(cmin, cmax);
			}
			ahn2HausdorffDistances.emplace(std::make_pair(std::make_pair(ahn2Index, ahn3Index), cmax));
		}
	}

	// TODO: add intermediate reporting into above loop
	if (progress)
		progress(0.35f, "AHN2 to AHN3 distances calculated.");

	// AHN3 -> AHN2
	for (const GUInt32 ahn3Index : AHN3ClusterMap.clusterIndexes())
	{
		for (const GUInt32 ahn2Index : AHN2ClusterMap.clusterIndexes())
		{
			auto ahn3Center = AHN3ClusterMap.center2D(ahn3Index);
			auto ahn2Center = AHN2ClusterMap.center2D(ahn2Index);
			double distance = ahn2Center.Distance(&ahn3Center);
			if (distance >= maximumDistance)
				continue;

			double cmax = 0;
			for (const OGRPoint& ahn3Point : AHN3ClusterMap.points(ahn3Index))
			{
				double cmin = std::numeric_limits<double>::max();
				for (const OGRPoint& ahn2Point : AHN2ClusterMap.points(ahn2Index))
				{
					double dist = ahn3Point.Distance(&ahn2Point);
					if (dist < cmax)
					{
						cmin = 0;
						break;
					}
					cmin = std::min(dist, cmin);
				}
				cmax = std::max(cmin, cmax);
			}
			ahn3HausdorffDistances.emplace(std::make_pair(std::make_pair(ahn3Index, ahn2Index), cmax));
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

		for (const GUInt32 ahn2Index : AHN2ClusterMap.clusterIndexes())
		{
			double dist = std::numeric_limits<double>::max();
			GUInt32 i = -1;

			if (std::find_if(closestClusters.begin(),
			                 closestClusters.end(),
			                 [&ahn2Index](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
			                 {
				                 return p.first.first == ahn2Index;
			                 })
			    == closestClusters.end())
			{
				for (const GUInt32 ahn3Index : AHN3ClusterMap.clusterIndexes())
				{
					auto ahn2Res = ahn2HausdorffDistances.find({ahn2Index, ahn3Index});
					auto ahn3Res = ahn3HausdorffDistances.find({ahn3Index, ahn2Index});

					if (ahn2Res != ahn2HausdorffDistances.end()
					    && ahn3Res != ahn3HausdorffDistances.end())
					{
						double newDist = std::max(ahn2Res->second, ahn3Res->second);

						if (newDist < dist &&
						    std::find_if(closestClusters.begin(),
						                 closestClusters.end(),
						                 [&ahn3Index](const std::pair<std::pair<GUInt32, GUInt32>, double>& p)
						                 {
							                 return p.first.second == ahn3Index;
						                 })
						    == closestClusters.end())
						{
							dist = newDist;
							i = ahn3Index;
						}
					}
				}

				if (dist <= maximumDistance)
				{
					ahn3Key.insert(std::make_pair(i, std::make_pair(ahn2Index, dist))); // AHN3 index is key
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

	for (GUInt32 ahn2Index : AHN2ClusterMap.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&ahn2Index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.first == ahn2Index;
		                 }) == closestClusters.end())
			lonelyClustersAHN2.push_back(ahn2Index);

	if (progress)
		progress(0.9f, "Lonely AHN2 clusters calculated.");

	for (GUInt32 ahn2Index : AHN3ClusterMap.clusterIndexes())
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&ahn2Index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.second == ahn2Index;
		                 }) == closestClusters.end())
			lonelyClustersAHN3.push_back(ahn2Index);

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


void HausdorffDistance::randomizePoints()
{
	AHN2ClusterMap.shuffle();
	AHN3ClusterMap.shuffle();
}
} // Vegetation
} // AHN