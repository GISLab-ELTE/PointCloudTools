#include "HausdorffDistance.h"

namespace AHN
{
namespace Vegetation
{
void HausdorffDistance::onExecute()
{
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

	for (auto iter = ahn2HausdorffDistances.begin(); iter != ahn2HausdorffDistances.end(); ++iter)
	{
		if (ahn3HausdorffDistances.count(iter->first) == 1 &&
		    std::find_if(closestClusters.begin(), closestClusters.end(),
		                 [&iter](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return iter->first.first == item.first.first;
		                 }) == closestClusters.end())
			closestClusters.emplace(*iter);
	}

	if (progress)
		progress(0.8f, "Cluster map pairs calculated.");

	for (GUInt32 index : AHN2ClusterMap.clusterIndexes())
		if (std::find_if(ahn2HausdorffDistances.begin(), ahn2HausdorffDistances.end(),
		                 [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.first == index;
		                 }) == ahn2HausdorffDistances.end())
			lonelyClustersAHN2.push_back(index);

	if (progress)
		progress(0.9f, "Lonely AHN2 clusters calculated.");

	for (GUInt32 index : AHN3ClusterMap.clusterIndexes())
		if (std::find_if(ahn2HausdorffDistances.begin(), ahn2HausdorffDistances.end(),
		                 [&index](const std::pair<std::pair<GUInt32, GUInt32>, double>& item)
		                 {
			                 return item.first.second == index;
		                 }) == ahn2HausdorffDistances.end())
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
