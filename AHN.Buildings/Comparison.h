#pragma once

#include <string>

#include <CloudTools.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a difference comparison for AHN-2 & AHN-3 datasets.
/// </summary>
class Comparison : public CloudTools::DEM::SweepLineTransformation<float>
{
public:
	/// <summary>
	/// Maximum threshold of change.
	/// </summary>
	/// <remarks>
	/// AHN elevation data is defined in meters.
	/// </remarks>
	double maximumThreshold = 1000;
	/// <summary>
	/// Minimum threshold of change.
	/// </summary>
	/// <remarks>
	/// AHN elevation data is defined in meters.
	/// The theoratical error-threshold of measurements between AHN-2 and AHN-3 is 0.35m.
	/// </remarks>
	double minimumThreshold = 0.4;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="ahn2Dataset">The AHN-2 dataset of the comparison.</param>
	/// <param name="ahn3Dataset">The AHN-3 dataset of the comparison.</param>
	/// <param name="targetPath">The target path of the comparison.</param>
	/// <param name="progress">he callback method to report progress.</param>
	Comparison(GDALDataset* ahn2Dataset, GDALDataset* ahn3Dataset,
	           const std::string& targetPath,
	           ProgressType progress = nullptr);
	
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="ahn2Dataset">The AHN-2 dataset of the comparison.</param>
	/// <param name="ahn3Dataset">The AHN-3 dataset of the comparison.</param>
	/// <param name="ahn2Filter">The AHN-2 building filter of the comparison.</param>
	/// <param name="ahn3Filter">The AHN-3 building filter of the comparison.</param>
	/// <param name="targetPath">The target path of the comparison.</param>
	/// <param name="progress">he callback method to report progress.</param>
	Comparison(GDALDataset* ahn2Dataset, GDALDataset* ahn3Dataset,
	           GDALDataset* ahn2Filter, GDALDataset* ahn3Filter,
	           const std::string& targetPath,
	           ProgressType progress = nullptr);

	Comparison(const Comparison&) = delete;
	Comparison& operator=(const Comparison&) = delete;
};
} // Buildings
} // AHN
