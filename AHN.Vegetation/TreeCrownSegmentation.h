#pragma once

#include <string>
#include <vector>

#include <CloudTools.DEM/DatasetTransformation.hpp>
#include <CloudTools.DEM/Helper.h>

namespace AHN
{
namespace Vegetation
{
class TreeCrownSegmentation : public CloudTools::DEM::DatasetTransformation<GUInt32, float>
{
public:
	/// <summary>
	/// The tree crown seed points of the algorithm.
	/// </summary>
	std::vector<CloudTools::DEM::Point> seedPoints;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the algorithm.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="seedPoints">The tree crown seed points.</param>
	/// <param name="progress">The callback method to report progress.</param>
	TreeCrownSegmentation(const std::string& sourcePath,
			              const std::string& targetPath,
			              const std::vector<CloudTools::DEM::Point>& seedPoints,
			              Operation::ProgressType progress = nullptr)
		: DatasetTransformation<GUInt32, float>({ sourcePath }, targetPath, nullptr, progress),
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
			              const std::string& targetPath,
						  const std::vector<CloudTools::DEM::Point>& seedPoints,
			              Operation::ProgressType progress = nullptr)
		: DatasetTransformation<GUInt32, float>({ sourceDataset }, targetPath, nullptr, progress),
		  seedPoints(seedPoints)
	{
		initialize();
	}

	TreeCrownSegmentation(const TreeCrownSegmentation&) = delete;
	TreeCrownSegmentation& operator=(const TreeCrownSegmentation&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};
} // Vegetation
} // AHN
