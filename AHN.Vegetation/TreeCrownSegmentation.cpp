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

		std::unordered_set<Point> neighbors;
		// TODO: extended the clusters in an iterative approach
		// TODO: extend the method not only to seed points
		for (GUInt32 index : clusterMap.clusterIndexes())
		{
			neighbors = clusterMap.getNeighbors(index);
			for (const Point& p : neighbors)
				if (this->hasSourceData(p.first, p.second))
					clusterMap.addPoint(index, p.first, p.second);

		}

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
