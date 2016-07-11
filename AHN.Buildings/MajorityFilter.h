#pragma once

#include <string>

#include <CloudLib.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a majority filter for DEM datasets.
/// </summary>
class MajorityFilter : public CloudLib::DEM::SweepLineTransformation<float>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="range">The range of surrounding data to involve.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MajorityFilter(GDALDataset* sourceDataset,
	               const std::string& targetPath,
	               int range,
	               ProgressType progress = nullptr);
	MajorityFilter(const MajorityFilter&) = delete;
	MajorityFilter& operator=(const MajorityFilter&) = delete;
};
} // Buildings
} // AHN
