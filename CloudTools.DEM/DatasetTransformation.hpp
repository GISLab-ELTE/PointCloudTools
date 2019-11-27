#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include "Transformation.h"
#include "Metadata.h"
#include "Helper.h"

namespace fs = boost::filesystem;

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a sweepline transformation on DEM datasets.
/// </summary>
template <typename TargetType, typename SourceType = TargetType>
class DatasetTransformation : public Transformation
{
public:
	typedef std::function<void(int, int)> ComputationType;
	ComputationType computation;

	/// <summary>
	/// The indices of bands to use respectively for each data source.
	/// </summary>
	std::vector<int> bands;

protected:
	std::vector<SourceType*> _sourceData;
	TargetType* _targetData;

private:
	std::vector<SourceType> _sourceNodataValue;

public:
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourcePaths">The source files of the transformation.</param>
	/// <param name="targetPath">The target file of the transformation.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	DatasetTransformation(const std::vector<std::string>& sourcePaths,
	                      const std::string& targetPath,
	                      ComputationType computation,
	                      ProgressType progress = nullptr)
		: Transformation(sourcePaths, targetPath, progress),
		computation(computation)
	{ }

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourceDatasets">The source datasets of the transformation.</param>
	/// <param name="targetPath">The target file of the transformation.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	DatasetTransformation(const std::vector<GDALDataset*>& sourceDatasets,
	                      const std::string& targetPath,
	                      ComputationType computation,
	                      ProgressType progress = nullptr)
		: Transformation(sourceDatasets, targetPath, progress),
		computation(computation)
	{ }

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <remarks>
	/// Target is in memory raster by default and can be retrieved by <see cref="target()"/>.
	/// </remarks>
	/// <param name="sourceDatasets">The source datasets of the transformation.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	DatasetTransformation(const std::vector<GDALDataset*>& sourceDatasets,
	                      ComputationType computation,
	                      ProgressType progress = nullptr)
		: DatasetTransformation(sourceDatasets, std::string(), computation, progress)
	{
		targetFormat = "MEM";
	}

	DatasetTransformation(const DatasetTransformation&) = delete;
	DatasetTransformation& operator=(const DatasetTransformation&) = delete;

	~DatasetTransformation();

protected:
	/// <summary>
	/// Produces the target file.
	/// </summary>
	void onExecute() override;

	SourceType sourceData(int index, int i, int j) const
	{
		if (!isValid(index, i, j))
			return _sourceNodataValue[index];
		return _sourceData[index][j * _sourceMetadata[index].rasterSizeX() + i];
	}

	SourceType sourceData(int i, int j) const
	{
		return sourceData(0, i, j);
	}

	TargetType targetData(int i, int j) const
	{
		if (!isValid(i, j))
			return nodataValue;
		return _targetData[j * _targetMetadata.rasterSizeX() + i];
	}

	void setTargetData(int i, int j, TargetType value)
	{
		if (isValid(i, j))
			_targetData[j * _targetMetadata.rasterSizeX() + i] = value;
	}

	bool hasSourceData(int index, int i, int j) const
	{
		if (!isValid(index, i, j))
			return false;
		return sourceData(index, i, j) != _sourceNodataValue[index];
	}

	bool hasSourceData(int i, int j) const
	{
		return hasSourceData(0, i, j);
	}

	bool hasTargetData(int i, int j) const
	{
		if (!isValid(i, j))
			return false;
		return targetData(i, j) != nodataValue;
	}

private:
	bool isValid(int i, int j) const
	{
		return i >= 0 && i < _targetMetadata.rasterSizeX() &&
			   j >= 0 && j < _targetMetadata.rasterSizeY();
	}

	bool isValid(int index, int i, int j) const
	{
		return index >= 0 && index < sourceCount() &&
			   i >= 0 && i < _sourceMetadata[index].rasterSizeX() &&
			   j >= 0 && j < _sourceMetadata[index].rasterSizeY();
	}
};

template <typename TargetType, typename SourceType>
void DatasetTransformation<TargetType, SourceType>::onExecute()
{
	if (!computation)
		throw std::logic_error("No computation method defined.");

	// Create and open the target file
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(targetFormat.c_str());
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(_targetPath) &&
		driver->Delete(_targetPath.c_str()) == CE_Failure &&
		!fs::remove(_targetPath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataType sourceType = gdalType<SourceType>();
	GDALDataType targetType = gdalType<TargetType>();

	char **targetParams = nullptr;
	for (auto& co : createOptions)
		targetParams = CSLSetNameValue(targetParams, co.first.c_str(), co.second.c_str());

	_targetDataset = driver->Create(_targetPath.c_str(),
		_targetMetadata.rasterSizeX(), _targetMetadata.rasterSizeY(), 1,
		targetType, targetParams);
	CSLDestroy(targetParams);
	if (_targetDataset == nullptr)
		throw std::runtime_error("Target file creation failed.");

	_targetDataset->SetGeoTransform(&_targetMetadata.geoTransform()[0]);
	if (_targetMetadata.reference().Validate() == OGRERR_NONE)
	{
		char *wkt;
		_targetMetadata.reference().exportToWkt(&wkt);
		_targetDataset->SetProjection(wkt);
		OGRFree(wkt);
	}

	// Determine computation progress steps
	int computationSteps = sourceCount() + 2;

	// Open and check bands
	std::vector<GDALRasterBand*> sourceBands(sourceCount());
	_sourceNodataValue.resize(sourceCount());
	for (unsigned int i = 0; i < sourceCount(); ++i)
	{
		long long bandIndex;
		if (bands.size() > i)
		{
			// Use manually defined band index
			bandIndex = bands[i];
		}
		else
		{
			// Default band index: multiplicity of same source
			bandIndex = _sourceOwnership
				? std::count(_sourcePaths.begin(), _sourcePaths.begin() + i, _sourcePaths[i]) + 1
				: std::count(_sourceDatasets.begin(), _sourceDatasets.begin() + i, _sourceDatasets[i]) + 1;
		}
		sourceBands[i] = _sourceDatasets[i]->GetRasterBand(static_cast<int>(bandIndex));
		_sourceNodataValue[i] = static_cast<SourceType>(sourceBands[i]->GetNoDataValue());
	}
	GDALRasterBand* targetBand = _targetDataset->GetRasterBand(1);
	targetBand->SetNoDataValue(nodataValue);

	if (strictTypes && std::any_of(sourceBands.begin(), sourceBands.end(),
		[sourceType](GDALRasterBand* band)
	{
		return band->GetRasterDataType() != sourceType;
	}))
		throw std::domain_error("The data type of a source band does not match with the given data type.");

	// Read sources
	_sourceData.resize(sourceCount());
	for (unsigned int i = 0; i < sourceCount(); ++i)
		_sourceData[i] = new SourceType[_sourceMetadata[i].rasterSizeX() * _sourceMetadata[i].rasterSizeY()];
	
	CPLErr ioResult = CE_None;
	for (unsigned int i = 0; i < sourceCount(); ++i)
	{
		ioResult = static_cast<CPLErr>(ioResult |
			sourceBands[i]->RasterIO(GF_Read,
				0, 0,
				_sourceMetadata[i].rasterSizeX(), _sourceMetadata[i].rasterSizeY(),
				_sourceData[i], 
				_sourceMetadata[i].rasterSizeX(), _sourceMetadata[i].rasterSizeY(),
				sourceType, 
				0, 0));

		if (progress)
			progress((i + 1) * 1.f / computationSteps, "Done reading source #" + std::to_string(i + 1));
	}
	if (ioResult != CE_None)
		throw std::runtime_error("Source read error occured.");

	// Compute target
	_targetData = new TargetType[_targetMetadata.rasterSizeX() * _targetMetadata.rasterSizeY()];
	std::fill_n(_targetData, _targetMetadata.rasterSizeX() * _targetMetadata.rasterSizeY(), nodataValue);

	ProgressType origProgress = progress;
	if (progress)
	{
		progress = [&origProgress, computationSteps](float complete, std::string message)
		{
			return origProgress((computationSteps - 2 + complete) * 1.f / computationSteps, message);
		};
	}

	computation(_targetMetadata.rasterSizeX(), _targetMetadata.rasterSizeY());

	progress = origProgress;
	if (progress)
		progress((computationSteps - 1) * 1.f / computationSteps, "Computation performed");

	// Write target
	ioResult = targetBand->RasterIO(GF_Write,
		0, 0,
		_targetMetadata.rasterSizeX(), _targetMetadata.rasterSizeY(),
		_targetData,
		_targetMetadata.rasterSizeX(), _targetMetadata.rasterSizeY(),
		targetType, 
		0, 0);
	if (ioResult != CE_None)
		throw std::runtime_error("Target write error occured.");

	if (progress)
		progress(1.f, "Target written");
}

template <typename TargetType, typename SourceType>
DatasetTransformation<TargetType, SourceType>::~DatasetTransformation()
{
	if (isExecuted())
	{
		for (int i = 0; i < sourceCount(); ++i)
			delete[] _sourceData[i];
		delete[] _targetData;
	}
}
} // DEM
} // CloudTools
