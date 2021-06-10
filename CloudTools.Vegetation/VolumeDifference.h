#pragma once

#include <map>

#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

using namespace CloudTools::DEM;

namespace CloudTools
{
namespace Vegetation
{
class VolumeDifference
{
public:
	double fullVolumeA, fullVolumeB;
	std::map<GUInt32, double> lonelyVolumeA, lonelyVolumeB;
	std::map<std::pair<GUInt32, GUInt32>, double> diffs;

	VolumeDifference(ClusterMap& clusterMapA,
	                 ClusterMap& clusterMapB,
	                 std::shared_ptr<DistanceCalculation> distance)
		: clusterMapA(clusterMapA), clusterMapB(clusterMapB), distance(distance)
	{
		calculateVolume();
	}

private:
	enum class Epoch
	{
		A, B
	};

	ClusterMap clusterMapA, clusterMapB;
	std::shared_ptr<DistanceCalculation> distance;

	void calculateVolume();

	std::pair<double, std::map<GUInt32, double>> calculateLonelyEpochVolume(Epoch epoch, ClusterMap& map);

	void calculateDifference();
};
} // Vegetation
} // CloudTools
