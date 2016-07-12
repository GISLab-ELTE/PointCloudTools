#pragma once

#include <string>

#include <CloudLib.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a noise filter for DEM datasets.
/// </summary>
class MorphologyFilter : public CloudLib::DEM::SweepLineTransformation<float>
{
public:
	enum Method
	{
		Dilation,
		Erosion
	};
	
	/// <summary>
	/// The applied morphology method.
	/// </summary>
	Method method;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="mode">The applied morphology method.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MorphologyFilter(GDALDataset* sourceDataset,
	                 const std::string& targetPath,
	                 Method method = Method::Dilation,
	                 ProgressType progress = nullptr);
	MorphologyFilter(const MorphologyFilter&) = delete;
	MorphologyFilter& operator=(const MorphologyFilter&) = delete;
};
} // Buildings
} // AHN
