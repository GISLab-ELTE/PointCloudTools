#pragma once

#include "CloudTools.Common/Operation.h"
#include "CloudTools.DEM/SweepLineTransformation.hpp"
#include <iostream>

namespace AHN
{
namespace Vegetation
{
class InterpolateNoData : public CloudTools::DEM::SweepLineTransformation<float>
{
public:
	float threshold = 0.5;

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourcePaths">The source files of the difference comparison.</param>
	/// <param name="targetPath">The target file of the difference comparison.</param>
	/// <param name="progress">The callback method to report progress.</param>
	InterpolateNoData(const std::vector<std::string>& sourcePaths,
	                  const std::string& targetPath,
	                  CloudTools::Operation::ProgressType progress = nullptr,
	                  float ratio = 0.5)
		: CloudTools::DEM::SweepLineTransformation<float>(sourcePaths, targetPath, 1, nullptr, progress),
		  threshold(ratio)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourceDatasets">The source datasets of the difference comparison.</param>
	/// <param name="targetPath">The target file of the difference comparison.</param>
	/// <param name="progress">The callback method to report progress.</param>
	InterpolateNoData(const std::vector<GDALDataset*>& sourceDatasets,
	                  const std::string& targetPath,
	                  CloudTools::Operation::ProgressType progress = nullptr,
	                  float ratio = 0.5)
		: CloudTools::DEM::SweepLineTransformation<float>(sourceDatasets, targetPath, 1, nullptr, progress),
		  threshold(ratio)
	{
		initialize();
	}

	InterpolateNoData(const InterpolateNoData&) = delete;

	InterpolateNoData& operator=(const InterpolateNoData&) = delete;

private:
	void initialize();
};
} // Vegetation
} // AHN
