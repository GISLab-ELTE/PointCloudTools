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
	double maximumDistance = 3.0;
	CloudTools::DEM::ClusterMap AHN2clusterMap;
	CloudTools::DEM::ClusterMap AHN3clusterMap;

	HausdorffDistance(CloudTools::DEM::ClusterMap& AHN2clusterMap,
		              CloudTools::DEM::ClusterMap& AHN3clusterMap,
					  Operation::ProgressType progress = nullptr)
		: AHN2clusterMap(AHN2clusterMap), AHN3clusterMap(AHN3clusterMap)
	{
		
	}

	double clusterDistance(GUInt32, GUInt32);
	GUInt32 closestCluster(GUInt32);
	std::map<std::pair<GUInt32, GUInt32>, double> distances();
    const std::map<std::pair<GUInt32, GUInt32>, double>& closest();

private:
	std::map<std::pair<GUInt32, GUInt32>, double> hausdorffDistances;
	std::map<std::pair<GUInt32, GUInt32>, double> closestClusters;

	void onPrepare() {}
	void onExecute();
};
}
}
