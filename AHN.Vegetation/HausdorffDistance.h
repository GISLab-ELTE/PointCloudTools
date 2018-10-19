#pragma once

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetTransformation.hpp>

namespace AHN
{
namespace Vegetation
{
class HausdorffDistance : public CloudTools::Operation
{
public:
	double maximumDistance = 8.0;
	CloudTools::DEM::ClusterMap& AHN2clusterMap;
	CloudTools::DEM::ClusterMap& AHN3clusterMap;

	HausdorffDistance(CloudTools::DEM::ClusterMap& AHN2clusterMap,
		              CloudTools::DEM::ClusterMap& AHN3clusterMap,
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
