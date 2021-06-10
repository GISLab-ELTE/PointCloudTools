#pragma once

#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

using namespace CloudTools::DEM;

namespace CloudTools
{
namespace Vegetation
{
class HeightDifference
{
public:
	ClusterMap clusterMapA, clusterMapB;
	DistanceCalculation* distance;

	HeightDifference(ClusterMap& clusterMapA,
	                 ClusterMap& clusterMapB,
	                 DistanceCalculation* distance)
		: clusterMapA(clusterMapA), clusterMapB(clusterMapB), distance(distance)
	{
		calculateDifference();
	}

private:
	void calculateDifference();
};
} // Vegetation
} // CloudTools
