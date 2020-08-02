#pragma once

#include <string>
#include <vector>
#include <set>

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
	double maxVerticalDistance = 14.0;
	double maxHorizontalDistance = 12.0;
	double increaseVerticalDistance = 1.0;

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
	struct PointComparator
	{
		bool operator()(const OGRPoint& a, const OGRPoint& b) const
		{
			if (a.getX() < b.getX())
				return true;
			else if (a.getX() > b.getX())
				return false;
			else
				return a.getY() < b.getY();
		}
	};

	CloudTools::DEM::ClusterMap clusters;

	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();

	std::set<OGRPoint, PointComparator> expandCluster(GUInt32 index, double vertical);
};
} // Vegetation
} // AHN
