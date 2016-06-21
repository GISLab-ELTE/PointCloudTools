#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include "CalculationBase.h"
#include "Window.h"
#include "Metadata.h"
#include "Helper.h"

namespace fs = boost::filesystem;

namespace CloudLib
{
namespace DEM
{
/// <summary>
/// Represents a sweepline calculation on DEM datasets.
/// </summary>
template <typename TargetType, typename SourceType = TargetType>
class SweepLine : public CalculationBase
{
public:
	typedef std::function<TargetType(int, int, const std::vector<Window<SourceType>>&)> ComputationType;
	ComputationType computation;

protected:
	int _range;

public:
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourcePaths">The source files of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="range">The range of surrounding data to involve in the computations.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	SweepLine(const std::vector<std::string>& sourcePaths,
	          const std::string& targetPath,
	          int range,
	          ComputationType computation,
	          ProgressType progress = nullptr);

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourcePaths">The source files of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	SweepLine(const std::vector<std::string>& sourcePaths,
	          const std::string& targetPath,
	          ComputationType computation,
	          ProgressType progress = nullptr)
		: SweepLine(sourcePaths, targetPath, 0, computation, progress)
	{ }

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <param name="sourceDatasets">The source datasets of the calculation.</param>
	/// <param name="targetPath">The target file of the calculation.</param>
	/// <param name="range">The range of surrounding data to involve in the computations.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	SweepLine(const std::vector<GDALDataset*>& sourceDatasets,
	          const std::string& targetPath,
	          int range,
	          ComputationType computation,
	          ProgressType progress = nullptr);

	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <remarks>
	/// Target is in memory raster by default and can be retrieved by <see cref="target()"/>.
	/// </remarks>
	/// <param name="sourceDatasets">The source datasets of the calculation.</param>
	/// <param name="range">The range of surrounding data to involve in the computations.</param>
	/// <param name="computation">The callback function for computation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	SweepLine(const std::vector<GDALDataset*>& sourceDatasets,
	          int range,
	          ComputationType computation,
	          ProgressType progress = nullptr)
		: SweepLine(sourceDatasets, std::string(), 0, computation, progress)
	{
		targetFormat = "MEM";
	}

	SweepLine(const SweepLine&) = delete;
	SweepLine& operator=(const SweepLine&) = delete;

	/// <summary>
	/// Gets the range of surrounding data to involve in the computations.
	/// </summary>
	/// <remarks>
	/// The window size equals to <c>2 * range + 1</c>.
	/// </remarks>
	int range() const { return _range; }

	/// <summary>
	/// Set the range of surrounding data to involve in the computations.
	/// </summary>
	/// <remarks>
	/// The window size equals to <c>2 * range + 1</c>.
	/// </remarks>
	void setRange(int value);

protected:
	/// <summary>
	/// Produces the target file.
	/// </summary>
	void onExecute() override;
};

template <typename TargetType, typename SourceType>
SweepLine<TargetType, SourceType>::SweepLine(const std::vector<std::string>& sourcePaths,
                                             const std::string& targetPath,
                                             int range,
                                             ComputationType computation,
                                             ProgressType progress)
	: CalculationBase(sourcePaths, targetPath, progress),
	  computation(computation)
{
	setRange(range);
}

template <typename TargetType, typename SourceType>
SweepLine<TargetType, SourceType>::SweepLine(const std::vector<GDALDataset*>& sourceDatasets,
                                             const std::string& targetPath,
                                             int range,
                                             ComputationType computation,
                                             ProgressType progress)
	: CalculationBase(sourceDatasets, targetPath, progress),
	  computation(computation)
{
	setRange(range);
}

template <typename TargetType, typename SourceType>
void SweepLine<TargetType, SourceType>::setRange(int value)
{
	if (value < 0)
		throw std::out_of_range("Range must be non-negative.");
	_range = value;
}

template <typename TargetType, typename SourceType>
void SweepLine<TargetType, SourceType>::onExecute()
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
	if (_targetMetadata.reference()->Validate() == OGRERR_NONE)
	{
		char *wkt;
		_targetMetadata.reference()->exportToWkt(&wkt);
		_targetDataset->SetProjection(wkt);
		OGRFree(wkt);
	}

	// Determine calculation progress steps
	int calculationSize = _targetMetadata.rasterSizeY();
	int calculationStep = calculationSize / 199;
	int calculationProgress = 0;

	// Open and check bands
	std::vector<GDALRasterBand*> sourceBands(sourceCount());
	for (unsigned int i = 0; i < sourceCount(); ++i)
	{
		// Default band index: multiplicity of same source
		int bandIndex = _sourceOwnership
			? std::count(_sourcePaths.begin(), _sourcePaths.begin() + i, _sourcePaths[i]) + 1
			: std::count(_sourceDatasets.begin(), _sourceDatasets.begin() + i, _sourceDatasets[i]) + 1;
		sourceBands[i] = _sourceDatasets[i]->GetRasterBand(bandIndex);
	}
	GDALRasterBand* targetBand = _targetDataset->GetRasterBand(1);
	targetBand->SetNoDataValue(nodataValue);

	if (strictTypes && std::any_of(sourceBands.begin(), sourceBands.end(),
		[sourceType](GDALRasterBand* band)
	{
		return band->GetRasterDataType() != sourceType;
	}))
		throw std::domain_error("The data type of a source band does not match with the given data type.");

	// Define windows
	int windowSize = 2 * _range + 1;
	std::vector<Window<SourceType>> dataWindows;
	dataWindows.reserve(sourceCount());

	// Read sources and compute target
	SourceType** sourceScanlines = new SourceType*[sourceCount()];
	for (unsigned int i = 0; i < sourceCount(); ++i)
		sourceScanlines[i] = new SourceType[_sourceMetadata[i].rasterSizeX() * windowSize];
	TargetType* targetScanline = new TargetType[_targetMetadata.rasterSizeX()];

	for (int y = 0; y < _targetMetadata.rasterSizeY(); ++y)
	{
		dataWindows.clear();
		for (unsigned int i = 0; i < sourceCount(); ++i)
		{
			int sourceOffsetX = static_cast<int>((_sourceMetadata[i].originX() - _targetMetadata.originX()) / std::abs(_targetMetadata.pixelSizeX()));
			int sourceOffsetY = static_cast<int>((_targetMetadata.originY() - _sourceMetadata[i].originY()) / std::abs(_targetMetadata.pixelSizeY()));

			if (y + _range >= sourceOffsetY &&
				y - _range < sourceOffsetY + _sourceMetadata[i].rasterSizeY())
			{
				int readOffsetX = 0;
				int readOffsetY = std::max(0, -sourceOffsetY + y - _range);
				int readSizeX = _sourceMetadata[i].rasterSizeX();
				int readSizeY = -readOffsetY + std::min(readOffsetY + windowSize, _sourceMetadata[i].rasterSizeY());

				sourceBands[i]->RasterIO(GF_Read,
					readOffsetX, readOffsetY,
					readSizeX, readSizeY,
					sourceScanlines[i], _sourceMetadata[i].rasterSizeX(), windowSize,
					sourceType, 0, 0);

				dataWindows.emplace_back(sourceScanlines[i], sourceBands[i]->GetNoDataValue(),
					readSizeX, readSizeY,
					sourceOffsetX + readOffsetX, sourceOffsetY + readOffsetY,
					0, y);
			}
			else
				dataWindows.emplace_back(sourceScanlines[i], sourceBands[i]->GetNoDataValue(),
				0, 0,
				sourceOffsetX, sourceOffsetY,
				0, y);
		}

		for (int x = 0; x < _targetMetadata.rasterSizeX(); ++x)
		{
			for (Window<SourceType>& window : dataWindows)
				window.centerX = x;
			targetScanline[x] = computation(x, y, dataWindows);
		}

		targetBand->RasterIO(GF_Write,
			0, y,
			_targetMetadata.rasterSizeX(), 1,
			targetScanline, _targetMetadata.rasterSizeX(), 1,
			targetType, 0, 0);

		if (progress && (calculationProgress++ % calculationStep == 0 || calculationProgress == calculationSize))
			progress(1.f * calculationProgress / calculationSize, std::string());
	}

	for (unsigned int i = 0; i < sourceCount(); ++i)
		delete[] sourceScanlines[i];
	delete[] sourceScanlines;
	delete[] targetScanline;
}
} // DEM
} // CloudLib
