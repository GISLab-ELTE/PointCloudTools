#pragma once

#include <string>
#include <vector>
#include <set>

#include <CloudTools.Common/Helper.h>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetCalculation.hpp>

namespace AHN
{
namespace Vegetation
{
class TreeCrownSegmentation : public CloudTools::DEM::DatasetCalculation<float>
{
public:
	/// <summary>
	/// The tree crown seed points of the algorithm.
	/// </summary>
	std::vector<OGRPoint> seedPoints;

public:
	double maxVerticalDistance = 14.0; // in meters
	double maxHorizontalDistance = 12.0; // in units of resolution (e.g. with 0.5m resolution it is 6 meters)
	double initialVerticalDistance = 2.0;  // in meters
	double increaseVerticalDistance = 2.0;  // in meters

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the algorithm.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="seedPoints">The tree crown seed points.</param>
	/// <param name="progress">The callback method to report progress.</param>
	TreeCrownSegmentation(const std::string& sourcePath,
	                      const std::vector<OGRPoint>& seedPoints,
	                      Operation::ProgressType progress = nullptr)
		: DatasetCalculation<float>({sourcePath}, nullptr, progress),
		  seedPoints(seedPoints)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the algorithm.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="seedPoints">The tree crown seed points.</param>
	/// <param name="progress">The callback method to report progress.</param>
	TreeCrownSegmentation(GDALDataset* sourceDataset,
	                      const std::vector<OGRPoint>& seedPoints,
	                      Operation::ProgressType progress = nullptr)
		: DatasetCalculation<float>({sourceDataset}, nullptr, progress),
		  seedPoints(seedPoints)
	{
		initialize();
	}

	TreeCrownSegmentation(const TreeCrownSegmentation&) = delete;

	TreeCrownSegmentation& operator=(const TreeCrownSegmentation&) = delete;

	CloudTools::DEM::ClusterMap& clusterMap();

private:
	CloudTools::DEM::ClusterMap clusters;

	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();

	std::set<OGRPoint, CloudTools::PointComparator> expandCluster(GUInt32 index, double verticalThreshold);
};
} // Vegetation
} // AHN
