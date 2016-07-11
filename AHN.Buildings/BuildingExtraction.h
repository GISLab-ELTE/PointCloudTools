#pragma once

#include <string>

#include <gdal.h>

#include <CloudLib.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a building (object) extractor from surface and terrain DEM datasets.
/// </summary>
/// <remarks>
/// The terrain dataset should be non-interpolated, containing nodata-value at the location of buildings.
/// </remarks>
class BuildingExtraction : public CloudLib::DEM::SweepLineTransformation<GByte, float>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="surfaceDataset">The surface dataset of the computation.</param>
	/// <param name="terrainDataset">The non-interpolated terrain dataset of the computation.</param>
	/// <param name="targetPath">The target filter map of the computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	BuildingExtraction(GDALDataset* surfaceDataset, GDALDataset* terrainDataset,
	                   const std::string& targetPath,
	                   ProgressType progress = nullptr);
	BuildingExtraction(const BuildingExtraction&) = delete;
	BuildingExtraction& operator=(const BuildingExtraction&) = delete;
};
} // Buildings
} // AHN
