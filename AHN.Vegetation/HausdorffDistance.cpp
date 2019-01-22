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
			for (const OGRPoint &point : AHN2clusterMap.points(index))
			{
				std::vector<OGRPoint> ahn3Points = AHN3clusterMap.points(otherIndex);
				minDistance = point.Distance(&ahn3Points[0]);
				for (const OGRPoint &otherPoint : ahn3Points)
				{
					double dist = point.Distance(&otherPoint);
					if (dist < minDistance)
					{
						minDistance = dist;
					}
				}
				distances.push_back(minDistance);
			}
			hausdorffDistances.emplace(std::make_pair(std::make_pair(index, otherIndex),
                              *std::max_element(distances.begin(), distances.end())));
		}
	}

	for (std::map<std::pair<GUInt32, GUInt32>, double>::iterator iter
			= hausdorffDistances.begin(); iter != hausdorffDistances.end(); iter++)
		if (std::find_if(closestClusters.begin(), closestClusters.end(),
						 [&iter](const std::pair<std::pair<GUInt32, GUInt32>, double> &item)
						 { return iter->first.first == item.first.first; }) == closestClusters.end())
			closestClusters.emplace(*iter);

	for(GUInt32 index : AHN2clusterMap.clusterIndexes())
		if(std::find_if(hausdorffDistances.begin(), hausdorffDistances.end(),
		[&index](const std::pair<std::pair<GUInt32, GUInt32>, double> &item)
		{ return item.first.first == index; }) == hausdorffDistances.end())
			lonelyClustersAHN2.push_back(index);

	for(GUInt32 index : AHN3clusterMap.clusterIndexes())
		if(std::find_if(hausdorffDistances.begin(), hausdorffDistances.end(),
		[&index](const std::pair<std::pair<GUInt32, GUInt32>, double> &item)
		{ return item.first.second == index; }) == hausdorffDistances.end())
			lonelyClustersAHN3.push_back(index);

    /*for (GUInt32 index : lonelyClustersAHN2.clusterIndexes())
      for (const OGRPoint& point : clusterMap.points(index))
      {
        this->setTargetData(point.getX(), point.getY(), index);
      }*/
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

std::map<std::pair<GUInt32, GUInt32>, double> HausdorffDistance::distances()
{
	return hausdorffDistances;
}

const std::map<std::pair<GUInt32, GUInt32>, double>& HausdorffDistance::closest()
{
	return closestClusters;
}

const std::vector<GUInt32>& HausdorffDistance::lonelyAHN2()
{
	return lonelyClustersAHN2;
}

const std::vector<GUInt32>& HausdorffDistance::lonelyAHN3()
{
	return lonelyClustersAHN3;
}
}
}