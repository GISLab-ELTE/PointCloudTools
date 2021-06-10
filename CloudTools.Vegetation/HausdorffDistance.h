#pragma once

#include <map>

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetTransformation.hpp>

#include "DistanceCalculation.h"

namespace CloudTools
{
namespace Vegetation
{
class HausdorffDistance : public DistanceCalculation
{
public:
	HausdorffDistance(CloudTools::DEM::ClusterMap& clusterMapA,
	                  CloudTools::DEM::ClusterMap& clusterMapB,
	                  double maximumDistance = 16.0, // in units of resolution (e.g. with 0.5m resolution it is 8 meters)
	                  Operation::ProgressType progress = nullptr)
		: DistanceCalculation(clusterMapA, clusterMapB, maximumDistance, progress)
	{
	}

	double clusterDistance(GUInt32, GUInt32);

	GUInt32 closestCluster(GUInt32);

	std::map<std::pair<GUInt32, GUInt32>, double> distances() const;

private:
	std::map<std::pair<GUInt32, GUInt32>, double> hausdorffDistancesA;
	std::map<std::pair<GUInt32, GUInt32>, double> hausdorffDistancesB;

	void onExecute() override;
	void randomizePoints();
};
} // Vegetation
} // CloudTools
