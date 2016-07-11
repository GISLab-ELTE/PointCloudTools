#pragma once

#include <string>

#include <CloudLib.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a difference comparison for AHN-2 & AHN-3 datasets.
/// </summary>
class Comparison : public CloudLib::DEM::SweepLineTransformation<float>
{
public:
	double maximumThreshold = 1000;
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
