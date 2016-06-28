#pragma once

#include <string>
#include <vector>
#include <map>

#include <gdal_priv.h>

#include "Calculation.h"

namespace CloudLib
{
namespace DEM
{
/// <summary>
/// Represents a transformation on DEM datasets.
/// </summary>
class Transformation : public Calculation
{
public:
	/// <summary>
	/// Target output format.
	/// </summary>
	/// <remarks>
	/// For supported formats, <see cref="http://www.gdal.org/formats_list.html" />.
	/// </remarks>
	std::string targetFormat = "GTiff";

	/// <summary>
	/// Format specific output creation options.
	/// </summary>
	/// <remarks>
	/// For supported options, <see cref="http://www.gdal.org/formats_list.html" />.
	/// </remarks>
	std::map<std::string, std::string> createOptions;

	/// <summary>
	/// The nodata value.
	/// </summary>
	/// <remarks>
	/// Default value as defined by GDAL (gdalrasterband.cpp).
	/// </remarks>
	double nodataValue = -1e10;

protected:
	std::string _targetPath;
	GDALDataset* _targetDataset;
	bool _targetOwnerShip = true;

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
		  _targetPath(targetPath), _targetDataset(nullptr)
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
		  _targetPath(targetPath), _targetDataset(nullptr)
	{ }

	Transformation(const Transformation&) = delete;
	Transformation& operator=(const Transformation&) = delete;
	~Transformation();
	
	/// <summary>
	/// Retrieves the target dataset.
	/// </summary>
	/// <remarks>
	/// By calling this method, the target datatset will be released by the transformation and won't be automatically freed.
	/// </remarks>
	/// <returns>The target dataset.</returns>
	GDALDataset* target();
};
} // DEM
} // CloudLib
