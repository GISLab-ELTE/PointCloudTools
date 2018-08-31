#pragma once

#include <string>

#include <CloudTools.DEM/ClusterMap.h>
#include "TreeCrownSegmentation.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
void TreeCrownSegmentation::initialize()
{
	this->nodataValue = 0;

	this->computation = [this](int sizeX, int sizeY)
	{
		ClusterMap clusterMap;

		// Create initial clusters from seed points
		for (const auto& point : this->seedPoints)
		{
			clusterMap.createCluster(point.first, point.second);
		}

		// TODO: extended the clusters in an iterative approach

		// Write out the clusters as a DEM
		for (GUInt32 index : clusterMap.clusterIndexes())
			for (const Point& point : clusterMap.points(index))
			{
				this->setTargetData(point.first, point.second, index);
			}

		if (this->progress)
			this->progress(1.f, "Target created");
	};
}
} // Vegetation
} // AHN
