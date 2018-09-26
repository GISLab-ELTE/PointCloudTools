#pragma once

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
          clusterMap.createCluster(point.getX(), point.getY());
      }

      std::vector<OGRPoint> neighbors;
      bool hasChanged;
      double currentVerticalDistance = 0.5;
      do
      {
        hasChanged = false;
        for (GUInt32 index : clusterMap.clusterIndexes())
        {
          neighbors = clusterMap.neighbors(index);
		  OGRPoint center = clusterMap.center(index);
		  for (const OGRPoint& p : neighbors)
		  {
			  double horizontalDistance = std::sqrt(std::pow(center.getX() - p.getX(), 2.0)
			      + std::pow(center.getY() - p.getY(), 2.0));
			  double verticalDistance = std::abs(sourceData(p.getX(), p.getY())
			      - sourceData(clusterMap.seedPoint(index).getX(), clusterMap.seedPoint(index).getY()));
			  if (this->hasSourceData(p.getX(), p.getY()) && horizontalDistance <= 4.0
                  && verticalDistance <= currentVerticalDistance)
			  {
				  clusterMap.addPoint(index, p.getX(), p.getY());
				  hasChanged = true;
			  }
		  }
        }
        currentVerticalDistance += increaseVerticalDistance;
      }
      while(hasChanged && currentVerticalDistance <= maxVerticalDistance);

      // Write out the clusters as a DEM
      for (GUInt32 index : clusterMap.clusterIndexes())
          for (const OGRPoint& point : clusterMap.points(index))
          {
              this->setTargetData(point.getX(), point.getY(), index);
          }

      if (this->progress)
          this->progress(1.f, "Target created");
	};
}
} // Vegetation
} // AHN
