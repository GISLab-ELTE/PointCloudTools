#pragma once

#include "CloudTools.Common/Operation.h"
#include "CloudTools.DEM/ClusterMap.h"

namespace AHN
{
namespace Vegetation
{
class DistanceCalculation : public CloudTools::Operation
{
public:
	double maximumDistance;
	CloudTools::DEM::ClusterMap AHN2ClusterMap;
	CloudTools::DEM::ClusterMap AHN3ClusterMap;

	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

	DistanceCalculation(const CloudTools::DEM::ClusterMap& AHN2clusterMap,
	                    const CloudTools::DEM::ClusterMap& AHN3clusterMap,
	                    double maximumDistance = 9.0,
	                    Operation::ProgressType progress = nullptr)
		: AHN2ClusterMap(AHN2clusterMap), AHN3ClusterMap(AHN3clusterMap), maximumDistance(maximumDistance),
		  progress(progress)
	{
	}

	const std::map<std::pair<GUInt32, GUInt32>, double>& closest() const
	{
		return closestClusters;
	}

	const std::vector<GUInt32>& lonelyAHN2() const
	{
		return lonelyClustersAHN2;
	}

	const std::vector<GUInt32>& lonelyAHN3() const
	{
		return lonelyClustersAHN3;
	}

	void onPrepare() override
	{}

protected:
	std::map<std::pair<GUInt32, GUInt32>, double> closestClusters;
	std::vector<GUInt32> lonelyClustersAHN2;
	std::vector<GUInt32> lonelyClustersAHN3;
};
} // Vegetation
} // AHN
