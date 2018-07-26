#pragma once

#include <string>

#include <gdal.h>

#include <CloudTools.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a building (artifical object) filter for DEM datasets.
/// </summary>
class BuildingFilter : public CloudTools::DEM::SweepLineTransformation<GByte, float>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="progress">The callback method to report progress.</param>
	BuildingFilter(GDALDataset* sourceDataset,
	               const std::string& targetPath,
	               ProgressType progress = nullptr);
	BuildingFilter(const BuildingFilter&) = delete;
	BuildingFilter& operator=(const BuildingFilter&) = delete;
};
} // Buildings
} // AHN
