#pragma once

#include <string>
#include <vector>

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
class Calculation : public Operation
{
public:
	/// <summary>
	/// The spatial reference system for the sources and the target.
	/// </summary>
	/// <remarks>
	/// Setting this property will override SRS detection.
	/// </remarks>
	std::string spatialReference;

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
	std::vector<GDALDataset*> _sourceDatasets;
	std::vector<RasterMetadata> _sourceMetadata;
	bool _sourceOwnership;

	RasterMetadata _targetMetadata;
public:
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourcePaths">The source files of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Calculation(const std::vector<std::string>& sourcePaths,
	            ProgressType progress = nullptr);

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourceDataset">The source datasets of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Calculation(const std::vector<GDALDataset*>& sourceDatasets,
	            ProgressType progress = nullptr);

	Calculation(const Calculation&) = delete;
	Calculation& operator=(const Calculation&) = delete;
	~Calculation();

	std::size_t sourceCount() const
	{
		return _sourceDatasets.size();
	}

	const RasterMetadata& sourceMetadata(unsigned int index) const;
	const RasterMetadata& sourceMetadata(const std::string& file) const;
	const RasterMetadata& targetMetadata() const;

protected:
	/// <summary>
	/// Verifies sources and calculates the metadata for the target.
	/// </summary>
	void onPrepare() override;
};
} // DEM
} // CloudLib
