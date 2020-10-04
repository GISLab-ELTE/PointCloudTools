#pragma once

#include <map>

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetTransformation.hpp>

#include "DistanceCalculation.h"

namespace AHN
{
namespace Vegetation
{
class HausdorffDistance : public DistanceCalculation
{
public:
	HausdorffDistance(CloudTools::DEM::ClusterMap& AHN2clusterMap,
	                  CloudTools::DEM::ClusterMap& AHN3clusterMap,
	                  double maximumDistance = 10.0, // in units of resolution (e.g. with 0.5m resolution it is 5 meters)
	                  Operation::ProgressType progress = nullptr)
		: DistanceCalculation(AHN2clusterMap, AHN3clusterMap, maximumDistance, progress)
	{
	}

	double clusterDistance(GUInt32, GUInt32);

	GUInt32 closestCluster(GUInt32);

	std::map<std::pair<GUInt32, GUInt32>, double> distances() const;

private:
	std::map<std::pair<GUInt32, GUInt32>, double> ahn2HausdorffDistances;
	std::map<std::pair<GUInt32, GUInt32>, double> ahn3HausdorffDistances;

	void onExecute() override;
};
} // Vegetation
} // AHN
