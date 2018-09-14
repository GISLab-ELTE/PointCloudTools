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
      int iteration = 0;
      bool hasChanged;
      do
      {
        hasChanged = false;
        for (GUInt32 index : clusterMap.clusterIndexes())
        {
          neighbors = clusterMap.neighbors(index);
          for (const OGRPoint& p : neighbors)
            if (this->hasSourceData(p.getX(), p.getY()))
            {
              clusterMap.addPoint(index, p.getX(), p.getY());
              hasChanged = true;
              ++iteration;
            }
        }
      }
      while(hasChanged && iteration <= 5);

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
