#pragma once

#include <vector>

#include "Calculation.h"
#include "Creation.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a transformation on DEM datasets.
/// </summary>
class Transformation : public Calculation, public Creation
{
public:
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourcePaths">The source files of the transformation.</param>
	/// <param name="targetPath">The target file of the transformation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Transformation(const std::vector<std::string>& sourcePaths,
	               const std::string& targetPath,
	               ProgressType progress = nullptr)
		: Calculation(sourcePaths, progress),
		  Creation(targetPath)
	{ }

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourceDataset">The source datasets of the transformation.</param>
	/// <param name="targetPath">The target file of the transformation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Transformation(const std::vector<GDALDataset*>& sourceDatasets,
	               const std::string& targetPath,
	               ProgressType progress = nullptr)
		: Calculation(sourceDatasets, progress),
		  Creation(targetPath)
	{ }

	Transformation(const Transformation&) = delete;
	Transformation& operator=(const Transformation&) = delete;
	~Transformation() { };
};
} // DEM
} // CloudTools
