#pragma once

#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetTransformation.hpp>

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
class HausdorffDistance : public CloudTools::DEM::DatasetTransformation<GUInt32, float>
{
public:
	double maximumDistance = 8.0;
	std::unordered_map<GUInt32, double> hausdorffDistances;
	ClusterMap& AHN2clusterMap;
	ClusterMap& AHN3clusterMap;

	HausdorffDistance(const std::string& sourcePath,
					  const std::string& targetPath,
					  ClusterMap& AHN2clusterMap,
					  ClusterMap& AHN3clusterMap,
					  Operation::ProgressType progress = nullptr)
		: DatasetTransformation<GUInt32, float>({ sourcePath }, targetPath, nullptr, progress),
		  AHN2clusterMap(AHN2clusterMap), AHN3clusterMap(AHN3clusterMap)
	{
		initialize();
	}

private:
	void initialize();
	double distanceOfPoints(OGRPoint, OGRPoint);
};
}
}
