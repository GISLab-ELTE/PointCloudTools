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
			double distance = AHN2clusterMap.center(index).Distance(&AHN3clusterMap.center(otherIndex));
			if (distance >= maximumDistance)
				continue;

			distances.clear();

			for (OGRPoint point : AHN2clusterMap.points(index))
			{
				std::vector<OGRPoint> ahn3Points = AHN3clusterMap.points(otherIndex);
				minDistance = point.Distance(&ahn3Points[0]);
				for (OGRPoint otherPoint : ahn3Points)
				{
					double dist = point.Distance(&otherPoint);
					if (dist < minDistance)
					{
						minDistance = dist;
					}
				}
				distances.emplace_back(minDistance);
			}

			hausdorffDistances.insert(std::make_pair(std::make_pair(index, otherIndex), *std::max_element(distances.begin(), distances.end())));
		}
	}
}
}
}