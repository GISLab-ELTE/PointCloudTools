#pragma once

#include <string>
#include <vector>

#include <gdal_priv.h>
#include <gdal_alg.h>

#include "../Transformation.h"
#include "../SweepLineTransformation.hpp"
#include "../Window.hpp"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a cluster filter for DEM datasets.
/// </summary>
template <typename DataType = float>
class ClusterFilter : public Transformation
{
public:	
	/// <summary>
	/// Cluster size threshold in pixels.
	/// </summary>
	/// <remarks>
	/// 400 pixels is 100m2 with 0.5m AHN raster grid.
	/// </remarks>
	int sizeThreshold = 400;	
	/// <summary>
	/// <c>true</c> to test connectedness diagonally, <c>false</c> otherwise.
	/// </summary>
	bool diagonalConnectedness = false;

protected:
	std::string _sievePath;
	GDALDataset* _sieveDataset;
	bool _sieveOwnerShip = true;

	ProgressType _progressBinarization;
	ProgressType _progressSieve;
	ProgressType _progressFilter;

public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="sourceDataset">The source path of the filter.</param>
	/// <param name="filterPath">The filter map file of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="progress">The callback method to report progress.</param>
	ClusterFilter(const std::string& sourcePath,
	              const std::string& filterPath,
	              const std::string& targetPath,
	              ProgressType progress = nullptr)
		: Transformation({sourcePath}, targetPath, progress),
		  _sievePath(filterPath), _sieveDataset(nullptr)
	{ }

	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="filterPath">The filter map file of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="progress">The callback method to report progress.</param>
	ClusterFilter(GDALDataset* sourceDataset,
		const std::string& filterPath,
		const std::string& targetPath,
		ProgressType progress = nullptr)
		: Transformation({ sourceDataset }, targetPath, progress),
		_sievePath(filterPath), _sieveDataset(nullptr)
	{ }

	ClusterFilter(const ClusterFilter&) = delete;
	ClusterFilter& operator=(const ClusterFilter&) = delete;
	~ClusterFilter();

	/// <summary>
	/// Retrieves the sieve filter dataset.
	/// </summary>
	/// <remarks>
	/// By calling this method, the sieve filter datatset will be released by the transformation and won't be automatically freed.
	/// </remarks>
	/// <returns>The sieve filter dataset.</returns>
	GDALDataset* filter();

protected:
	/// <summary>
	/// Verifies source and calculates the metadata for the target.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Produces the target.
	/// </summary>
	void onExecute() override;

private:
	/// <summary>
	/// Routes the C-style GDAL progress reports to the defined reporter.
	/// </summary>
	static int CPL_STDCALL gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg);
};

template <typename DataType>
ClusterFilter<DataType>::~ClusterFilter()
{
	if (_sieveOwnerShip && _sieveDataset != nullptr)
		GDALClose(_sieveDataset);
}

template <typename DataType>
GDALDataset* ClusterFilter<DataType>::filter()
{
	if (!isExecuted())
		throw std::logic_error("The computation is not executed.");
	_sieveOwnerShip = false;
	return _sieveDataset;
}

template <typename DataType>
void ClusterFilter<DataType>::onPrepare()
{
	Transformation::onPrepare();

	if (this->progress)
	{
		_progressBinarization = [this](float complete, const std::string &message)
		{
			return this->progress(complete / 4, message);
		};
		_progressSieve = [this](float complete, const std::string &message)
		{
			return this->progress(.25f + complete / 2, message);
		};
		_progressFilter = [this](float complete, const std::string &message)
		{
			return this->progress(.75f + complete / 4, message);
		};
	}
	else
		_progressBinarization = _progressSieve = _progressFilter = nullptr;
}

template <typename DataType>
void ClusterFilter<DataType>::onExecute()
{
	// Binarization of input data
	SweepLineTransformation<GByte, DataType> binarization(
		{ _sourceDatasets[0] }, _sievePath, 0,
		[](int x, int y, const std::vector<Window<DataType>>& sources)
	{
		const Window<DataType>& source = sources[0];
		if (!source.hasData()) return 1;
		else return 255;
	},
		_progressBinarization);
	binarization.nodataValue = 0;
	binarization.targetFormat = targetFormat;
	binarization.createOptions = createOptions;
	binarization.spatialReference = spatialReference;
	binarization.execute();
	_sieveDataset = binarization.target();

	// Sieve filter
	GDALRasterBand* band = _sieveDataset->GetRasterBand(1);
	GDALSieveFilter(
		band, nullptr, band,
		this->sizeThreshold, this->diagonalConnectedness ? 8 : 4,
		nullptr, gdalProgress, static_cast<void*>(_progressSieve ? &_progressSieve : nullptr));

	// Apply sieve filter on input
	SweepLineTransformation<DataType> filter(std::vector<GDALDataset*>{_sourceDatasets[0], _sieveDataset},
		_targetPath, 0, nullptr, _progressFilter);
	filter.computation = [&filter]
	(int x, int y, const std::vector<Window<DataType>>& sources)
	{
		const Window<DataType>& data = sources[0];
		const Window<DataType>& sieve = sources[1];

		if (sieve.hasData() && sieve.data() == 255) return data.data();
		else return static_cast<DataType>(filter.nodataValue);
	};
	filter.nodataValue = nodataValue;
	filter.targetFormat = targetFormat;
	filter.createOptions = createOptions;
	filter.spatialReference = spatialReference;
	filter.execute();
	_targetDataset = filter.target();
}

template <typename DataType>
int ClusterFilter<DataType>::gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg)
{
	ProgressType* progress = static_cast<ProgressType*>(pProgressArg);
	if (!progress) return true;
	return (*progress)(static_cast<float>(dfComplete), pszMessage);
}
} // DEM
} // CloudTools
