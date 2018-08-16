#pragma once

#include <string>

#include <CloudTools.DEM/SweepLineTransformation.hpp>

namespace AHN
{
namespace Vegetation
{
/// <summary>
/// Represents a noise filter for DEM datasets.
/// </summary>
class NoiseFilter : public CloudTools::DEM::SweepLineTransformation<float>
{
public:	
	/// <summary>
	/// The threshold of noise in percentage (between values 0 and 1).
	/// </summary>
	/// <remarks>
	/// Noise is the average percentage of difference compared to the surrounding area.
	/// </remarks>
	double threshold = 0.5;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="range">The range of surrounding data to involve.</param>
	/// <param name="progress">The callback method to report progress.</param>
	NoiseFilter(GDALDataset *sourceDataset,
	            const std::string& targetPath,
				int range,
	            ProgressType progress = nullptr);
	NoiseFilter(const NoiseFilter&) = delete;
	NoiseFilter& operator=(const NoiseFilter&) = delete;
};
} // Buildings
} // AHN
