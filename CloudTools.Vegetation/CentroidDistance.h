#pragma once

#include <map>

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

namespace CloudTools
{
namespace Vegetation
{
class CentroidDistance : public DistanceCalculation
{
public:
	CentroidDistance(CloudTools::DEM::ClusterMap& clusterMapA,
	                 CloudTools::DEM::ClusterMap& clusterMapB,
	                 double maximumDistance = 10.0, // in units of resolution (e.g. with 0.5m resolution it is 5 meters)
	                 Operation::ProgressType progress = nullptr)
		: DistanceCalculation(clusterMapA, clusterMapB, maximumDistance, progress)
	{
	}

private:
	void onExecute() override;
};
} // Vegetation
} // CloudTools
