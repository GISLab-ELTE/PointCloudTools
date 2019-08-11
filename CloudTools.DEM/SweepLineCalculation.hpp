#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include <stdexcept>

#include <boost/filesystem.hpp>

#include "Calculation.h"
#include "Window.hpp"
#include "Metadata.h"
#include "Helper.h"

namespace fs = boost::filesystem;

namespace CloudTools
{
namespace DEM
{
	/// <summary>
	/// Represents a sweepline calculation on DEM datasets.
	/// </summary>
	template <typename SourceType>
	class SweepLineCalculation : public Calculation
	{
	public:
		typedef std::function<void(int, int, const std::vector<Window<SourceType>>&)> ComputationType;
		ComputationType computation;
		/// <summary>
		/// The indices of bands to use respectively for each data source.
		/// </summary>
		std::vector<int> bands;

	protected:
		int _range;

	public:
		/// <summary>
		/// Initializes a new instance of the class and loads source metadata.
		/// </summary>
		/// <param name="sourcePaths">The source files of the calculation.</param>
		/// <param name="range">The range of surrounding data to involve in the computations.</param>
		/// <param name="computation">The callback function for computation.</param>
		/// <param name="progress">The callback method to report progress.</param>
		SweepLineCalculation(const std::vector<std::string>& sourcePaths,
			                 int range,
			                 ComputationType computation,
			                 ProgressType progress = nullptr);

		/// <summary>
		/// Initializes a new instance of the class and loads source metadata.
		/// </summary>
		/// <param name="sourcePaths">The source files of the calculation.</param>
		/// <param name="computation">The callback function for computation.</param>
		/// <param name="progress">The callback method to report progress.</param>
		SweepLineCalculation(const std::vector<std::string>& sourcePaths,
			                 ComputationType computation,
			                 ProgressType progress = nullptr)
			: SweepLineCalculation(sourcePaths, 0, computation, progress)
		{ }

		/// <summary>
		/// Initializes a new instance of the class and loads source metadata.
		/// </summary>
		/// <param name="sourceDatasets">The source datasets of the calculation.</param>
		/// <param name="range">The range of surrounding data to involve in the computations.</param>
		/// <param name="computation">The callback function for computation.</param>
		/// <param name="progress">The callback method to report progress.</param>
		SweepLineCalculation(const std::vector<GDALDataset*>& sourceDatasets,
			                 int range,
			                 ComputationType computation,
			                 ProgressType progress = nullptr);

		SweepLineCalculation(const SweepLineCalculation&) = delete;
		SweepLineCalculation& operator=(const SweepLineCalculation&) = delete;

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
		/// Executes the computation on the target area.
		/// </summary>
		void onExecute() override;
	};

	template <typename SourceType>
	SweepLineCalculation<SourceType>::SweepLineCalculation(
		const std::vector<std::string>& sourcePaths,
		int range,
		ComputationType computation,
		ProgressType progress)
		: Calculation(sourcePaths, progress),
		computation(computation)
	{
		setRange(range);
	}

	template <typename SourceType>
	SweepLineCalculation<SourceType>::SweepLineCalculation(
		const std::vector<GDALDataset*>& sourceDatasets,
		int range,
		ComputationType computation,
		ProgressType progress)
		: Calculation(sourceDatasets, progress),
		computation(computation)
	{
		setRange(range);
	}

	template <typename SourceType>
	void SweepLineCalculation<SourceType>::setRange(int value)
	{
		if (value < 0)
			throw std::out_of_range("Range must be non-negative.");
		_range = value;
	}

	template <typename SourceType>
	void SweepLineCalculation<SourceType>::onExecute()
	{
		if (!computation)
			throw std::logic_error("No computation method defined.");

		// Determine computation progress steps
		int computationSize = _targetMetadata.rasterSizeY();
		int computationStep = computationSize / 199;
		int computationProgress = 0;

		// Open and check bands
		std::vector<GDALRasterBand*> sourceBands(sourceCount());
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
		}

		GDALDataType sourceType = gdalType<SourceType>();
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

		// Read sources and execute computation
		SourceType** sourceScanlines = new SourceType*[sourceCount()];
		for (unsigned int i = 0; i < sourceCount(); ++i)
			sourceScanlines[i] = new SourceType[_sourceMetadata[i].rasterSizeX() * windowSize];

		for (int y = 0; y < _targetMetadata.rasterSizeY(); ++y)
		{
			CPLErr ioResult = CE_None;

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

					ioResult = static_cast<CPLErr>(ioResult | 
						sourceBands[i]->RasterIO(GF_Read,
							readOffsetX, readOffsetY,
							readSizeX, readSizeY,
							sourceScanlines[i], _sourceMetadata[i].rasterSizeX(), windowSize,
							sourceType, 0, 0));

					dataWindows.emplace_back(sourceScanlines[i], 
						static_cast<SourceType>(sourceBands[i]->GetNoDataValue()),
						readSizeX, readSizeY,
						sourceOffsetX + readOffsetX, sourceOffsetY + readOffsetY,
						0, y);
				}
				else
					dataWindows.emplace_back(sourceScanlines[i], 
						static_cast<SourceType>(sourceBands[i]->GetNoDataValue()),
						0, 0,
						sourceOffsetX, sourceOffsetY,
						0, y);
			}
			if (ioResult != CE_None)
				throw std::runtime_error("Source read error occured.");

			for (int x = 0; x < _targetMetadata.rasterSizeX(); ++x)
			{
				for (Window<SourceType>& window : dataWindows)
					window.centerX = x;
				computation(x, y, dataWindows);
			}

			if (progress && (computationProgress++ % computationStep == 0 || computationProgress == computationSize))
				progress(1.f * computationProgress / computationSize, std::string());
		}

		for (unsigned int i = 0; i < sourceCount(); ++i)
			delete[] sourceScanlines[i];
		delete[] sourceScanlines;
	}
} // DEM
} // CloudTools
