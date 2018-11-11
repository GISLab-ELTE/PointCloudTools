#include "HausdorffDistance.h"

namespace AHN
{
namespace Vegetation
{
void HausdorffDistance::onExecute()
{
	std::vector<double> distances;
	double minDistance;

	for (GUInt32 index : AHN2clusterMap.clusterIndexes())
	{
		for (GUInt32 otherIndex : AHN3clusterMap.clusterIndexes())
		{
			auto c = AHN3clusterMap.center(otherIndex);
			double distance = AHN2clusterMap.center(index).Distance(&c);
			if (distance >= maximumDistance)
				continue;

			distances.clear();

			for (const OGRPoint& point : AHN2clusterMap.points(index))
			{
				std::vector<OGRPoint> ahn3Points = AHN3clusterMap.points(otherIndex);
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

			hausdorffDistances.emplace(std::make_pair(std::make_pair(index, otherIndex), *std::max_element(distances.begin(), distances.end())));
		}
	}
}

double HausdorffDistance::clusterDistance(GUInt32 index1, GUInt32 index2)
{
	std::pair<GUInt32, GUInt32> indexPair = std::make_pair(index1, index2);
	if (hausdorffDistances.find(indexPair) != hausdorffDistances.end())
		return hausdorffDistances.at(indexPair);
}

GUInt32 HausdorffDistance::closestCluster(GUInt32 index)
{
	GUInt32 closest = index;
	std::map<double, GUInt32> distances;
	for (auto elem : hausdorffDistances)
		if (elem.first.first == index)
			distances.emplace(std::make_pair(elem.second, elem.first.second));

	closest = distances.begin()->second;
	return closest;
}

std::unordered_map<std::pair<GUInt32, GUInt32>, double> HausdorffDistance::distances()
{
	return hausdorffDistances;
}

}
}