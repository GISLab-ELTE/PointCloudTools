#pragma once

#include <string>
#include <map>

#include <CloudTools.Common/Operation.h>
#include "Metadata.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a creation of a DEM dataset.
/// </summary>
class Creation : public virtual Operation
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
	GDALDataset* _targetDataset = nullptr;
	bool _targetOwnerShip = true;

public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="targetPath">The target file of the creation.</param>
	Creation(const std::string& targetPath)
		  : _targetPath(targetPath)
	{ }

	Creation(const Creation&) = delete;
	Creation& operator=(const Creation&) = delete;
	~Creation();

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
} // CloudTools
