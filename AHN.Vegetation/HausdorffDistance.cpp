#include "HausdorffDistance.h"

namespace AHN
{
namespace Vegetation
{
void HausdorffDistance::initialize()
{
	this->nodataValue = 0;

	this->computation = [this](int sizeX, int sizeY)
	{
		std::vector<double> distances;
		double minDistance;

		for (GUInt32 index : AHN2clusterMap.clusterIndexes())
		{
			if (std::find(AHN3clusterMap.clusterIndexes().begin(), AHN3clusterMap.clusterIndexes().end(), index) != AHN2clusterMap.clusterIndexes().end())
			{
				double distance = std::sqrt(std::pow(AHN2clusterMap.center(index).getX() - AHN3clusterMap.center(index).getX(), 2.0)
					+ std::pow(AHN3clusterMap.center(index).getY() - AHN3clusterMap.center(index).getY(), 2.0));
				if (distance >= maximumDistance)
					continue;

				distances.clear();
				distances.resize(0);

				for (OGRPoint point : AHN2clusterMap.points(index))
				{
					minDistance = distanceOfPoints(point, AHN3clusterMap.center(index));
					for (OGRPoint otherPoint : AHN3clusterMap.points(index))
					{
						double dist = distanceOfPoints(point, otherPoint);
						if (dist < minDistance)
						{
							minDistance = dist;
						}
					}
					distances.push_back(minDistance);
				}

				hausdorffDistances.insert(std::make_pair(index, *std::max_element(distances.begin(), distances.end())));
			}

		}

	};
}

double HausdorffDistance::distanceOfPoints(OGRPoint p1, OGRPoint p2)
{
	return std::sqrt(std::pow(p1.getX() - p2.getX(), 2.0)
		+ std::pow(p1.getY() - p2.getY(), 2.0));
}
}
}