#pragma once

#include <map>

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>

namespace CloudTools
{
namespace Vegetation
{
class DistanceCalculation : public CloudTools::Operation
{
public:
	double maximumDistance;
	CloudTools::DEM::ClusterMap clusterMapA;
	CloudTools::DEM::ClusterMap clusterMapB;

	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

	DistanceCalculation(const CloudTools::DEM::ClusterMap& clusterMapA,
	                    const CloudTools::DEM::ClusterMap& clusterMapB,
	                    double maximumDistance = 10.0, // in units of resolution (e.g. with 0.5m resolution it is 5 meters)
	                    Operation::ProgressType progress = nullptr)
		: clusterMapA(clusterMapA), clusterMapB(clusterMapB), maximumDistance(maximumDistance),
		  progress(progress)
	{
	}

	const std::map<std::pair<GUInt32, GUInt32>, double>& closest() const
	{
		return closestClusters;
	}

	const std::vector<GUInt32>& lonelyA() const
	{
		return lonelyClustersA;
	}

	const std::vector<GUInt32>& lonelyB() const
	{
		return lonelyClustersB;
	}

	void onPrepare() override
	{}

protected:
	std::map<std::pair<GUInt32, GUInt32>, double> closestClusters;
	std::vector<GUInt32> lonelyClustersA;
	std::vector<GUInt32> lonelyClustersB;
};
} // Vegetation
} // CloudTools
