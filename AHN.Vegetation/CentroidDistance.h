#pragma once

#include <map>

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

namespace AHN
{
namespace Vegetation
{
class CentroidDistance : public DistanceCalculation
{
public:
	CentroidDistance(CloudTools::DEM::ClusterMap& AHN2clusterMap,
	                 CloudTools::DEM::ClusterMap& AHN3clusterMap,
	                 double maximumDistance = 9.0,
	                 Operation::ProgressType progress = nullptr)
		: DistanceCalculation(AHN2clusterMap, AHN3clusterMap, maximumDistance, progress)
	{
	}

private:
	void onExecute() override;
};
} // Vegetation
} // AHN
