#pragma once

#include "TreeCrownSegmentation.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
	void TreeCrownSegmentation::onExecute()
	{	
		this->computation = [this](int sizeX, int sizeY)
		{
		  // Create initial clusters from seed points
		  for (const auto& point : this->seedPoints)
		  {
			  clusters.createCluster(point.getX(), point.getY());
		  }

		  std::vector<OGRPoint> neighbors;
		  bool hasChanged;
		  double currentVerticalDistance = 0.5;
		  do
		  {
			hasChanged = false;
			for (GUInt32 index : clusters.clusterIndexes())
			{
			  neighbors = clusters.neighbors(index);
			  OGRPoint center = clusters.center(index);
			  for (const OGRPoint& p : neighbors)
			  {
				  double horizontalDistance = std::sqrt(std::pow(center.getX() - p.getX(), 2.0)
					  + std::pow(center.getY() - p.getY(), 2.0));
				  double verticalDistance = std::abs(sourceData(p.getX(), p.getY())
					  - sourceData(clusters.seedPoint(index).getX(), clusters.seedPoint(index).getY()));
				  if (this->hasSourceData(p.getX(), p.getY()) && horizontalDistance <= 8.0
					  && verticalDistance <= currentVerticalDistance)
				  {
					  clusters.addPoint(index, p.getX(), p.getY());
					  hasChanged = true;
				  }
			  }
			}
			currentVerticalDistance += increaseVerticalDistance;
		  }
		  while(hasChanged && currentVerticalDistance <= maxVerticalDistance);

		  // Write out the clusters as a DEM
		  /*for (GUInt32 index : clusterMap.clusterIndexes())
			  for (const OGRPoint& point : clusterMap.points(index))
			  {
				  this->setTargetData(point.getX(), point.getY(), index);
			  }
			  */
		  if (this->progress)
			  this->progress(1.f, "Target created");
		};
}

ClusterMap& TreeCrownSegmentation::clusterMap()
{
	return this->clusters;
}
} // Vegetation
} // AHN
