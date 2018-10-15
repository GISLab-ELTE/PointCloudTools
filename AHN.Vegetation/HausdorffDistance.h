#pragma once

#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetTransformation.hpp>

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
class HausdorffDistance : public CloudTools::DEM::Operation
{
public:
	double maximumDistance = 8.0;
	ClusterMap& AHN2clusterMap;
	ClusterMap& AHN3clusterMap;

	HausdorffDistance(ClusterMap& AHN2clusterMap,
					  ClusterMap& AHN3clusterMap,
					  Operation::ProgressType progress = nullptr)
		: AHN2clusterMap(AHN2clusterMap), AHN3clusterMap(AHN3clusterMap)
	{
		onExecute();
	}

	double clusterDistance(GUInt32, GUInt32);
	GUInt32 closestCluster(GUInt32);

private:
	std::unordered_map<std::pair<GUInt32, GUInt32>, double> hausdorffDistances;

	void onPrepare() {}
	void onExecute();
};
}
}
