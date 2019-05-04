#ifndef CLOUDTOOLS_GRAVITYDISTANCE_H
#define CLOUDTOOLS_GRAVITYDISTANCE_H

#include <map>

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>

namespace AHN
{
namespace Vegetation
{
class GravityDistance : public CloudTools::Operation
{
public:
	double maximumDistance = 9.0;
	CloudTools::DEM::ClusterMap AHN2clusterMap;
	CloudTools::DEM::ClusterMap AHN3clusterMap;

	GravityDistance(CloudTools::DEM::ClusterMap& AHN2clusterMap,
	                CloudTools::DEM::ClusterMap& AHN3clusterMap,
	                Operation::ProgressType progress = nullptr)
		: AHN2clusterMap(AHN2clusterMap), AHN3clusterMap(AHN3clusterMap)
	{
	}

	double clusterDistance(GUInt32, GUInt32);
	GUInt32 closestCluster(GUInt32);

	const std::map<std::pair<GUInt32, GUInt32>, double>& closest();
	const std::vector<GUInt32>& lonelyAHN2();
	const std::vector<GUInt32>& lonelyAHN3();

private:
	std::map<std::pair<GUInt32, GUInt32>, double> gravityDistances;
	std::map<std::pair<GUInt32, GUInt32>, double> closestClusters;
	std::vector<GUInt32> lonelyClustersAHN2;
	std::vector<GUInt32> lonelyClustersAHN3;

	void onPrepare()
	{
	}

	void onExecute();
};
}
}

#endif //CLOUDTOOLS_GRAVITYDISTANCE_H
