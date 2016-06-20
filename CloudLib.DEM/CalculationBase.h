#pragma once

#include <string>
#include <vector>
#include <map>

#include <gdal_priv.h>

#include "Operation.h"
#include "Metadata.h"

namespace CloudLib
{
namespace DEM
{
/// <summary>
/// Represents a calculation on DEM datasets.
/// </summary>
class CalculationBase : public Operation
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
	/// The spatial reference system for the source and target files.
	/// </summary>
	/// <remarks>
	/// Setting this property will override SRS detection.
	/// </remarks>
	std::string spatialReference;

	/// <summary>
	/// The nodata value.
	/// </summary>
	/// <remarks>
	/// Default value as defined by GDAL (gdalrasterband.cpp).
	/// </remarks>
	double nodataValue = -1e10;

	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;
	
	/// <summary>
	/// Enforces strict data type matching for the source files.
	/// </summary>
	bool strictTypes = false;

protected:
	std::vector<std::string> _sourcePaths;
	std::string _targetPath;

	std::vector<GDALDataset*> _sourceDatasets;
	GDALDataset* _targetDataset;

	std::vector<RasterMetadata> _sourceMetadata;
	RasterMetadata _targetMetadata;

	bool _sourceOwnership;
	mutable bool _targetOwnerShip = true;

public:
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourcePaths">The source files of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	CalculationBase(const std::vector<std::string>& sourcePaths,
	                const std::string& targetPath,
	                ProgressType progress = nullptr);
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourceDataset">The source datasets of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	CalculationBase(const std::vector<GDALDataset*>& sourceDatasets,
	                const std::string& targetPath,
	                ProgressType progress = nullptr);
	CalculationBase(const CalculationBase&) = delete;
	CalculationBase& operator=(const CalculationBase&) = delete;
	~CalculationBase();

	unsigned int sourceCount() const { return _sourceDatasets.size(); }
	const RasterMetadata& sourceMetadata(unsigned int index) const;
	const RasterMetadata& sourceMetadata(const std::string& file) const;
	const RasterMetadata& targetMetadata() const;
	
	/// <summary>
	/// Retrieves the target dataset.
	/// </summary>
	/// <remarks>
	/// By calling this method, the target datatset will be released by the calculation and won't be automatically freed.
	/// </remarks>
	/// <returns>The target dataset.</returns>
	const GDALDataset* target() const;

protected:
	/// <summary>
	/// Verifies sources and calculates the metadata for the target.
	/// </summary>
	void onPrepare() override;
};
} // DEM
} // CloudLib
