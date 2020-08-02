#pragma once

#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
class HeightDifference
{
public:
	ClusterMap ahn2, ahn3;
	DistanceCalculation* distance;

	HeightDifference(ClusterMap& ahn2_map,
	                 ClusterMap& ahn3_map,
	                 DistanceCalculation* distance)
		: ahn2(ahn2_map), ahn3(ahn3_map), distance(distance)
	{
		calculateDifference();
	}

private:
	void calculateDifference();
};
} // Vegetation
} // AHN
