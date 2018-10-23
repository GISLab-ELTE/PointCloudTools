#pragma once

#include <string>

#include "MorphologyClusterFilter.h"

namespace AHN
{
namespace Vegetation
{
void MorphologyClusterFilter::onExecute()
{
	if (this->method == Method::Dilation && this->threshold == -1)
		this->threshold = 0;
	if (this->method == Method::Erosion && this->threshold == -1)
		this->threshold = 9;

	int counter;

	if (this->method == Method::Erosion)
		for (GUInt32 index : clusterMap.clusterIndexes())
			for (const OGRPoint& p : clusterMap.points(index))
			{
				counter = 0;
				for (int i = p.getX() - 1; i <= p.getX() + 1; i++)
					for (int j = p.getY() - 1; j <= p.getY() + 1; j++)
						if (clusterMap.clusterIndex(i, j) == clusterMap.clusterIndex(p.getX(), p.getY()))
							++counter;

				if (counter < this->threshold)
					clusterMap.removePoint(index, p.getX(), p.getY());
			}

	if(this->method == Method::Dilation){}
}

CloudTools::DEM::ClusterMap& MorphologyClusterFilter::target()
{
	return this->targetMap;
}

} //Vegetation
} //AHN