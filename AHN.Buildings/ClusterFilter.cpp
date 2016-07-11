#include <vector>

#include <gdal_priv.h>
#include <gdal_alg.h>

#include <CloudLib.DEM/Window.h>
#include "ClusterFilter.h"

using namespace CloudLib::DEM;

namespace AHN
{
namespace Buildings
{
ClusterFilter::~ClusterFilter()
{
	if (_sieveOwnerShip && _sieveDataset != nullptr)
		GDALClose(_sieveDataset);
}

GDALDataset* ClusterFilter::filter()
{
	if (!isExecuted())
		throw std::logic_error("The computation is not executed.");
	_sieveOwnerShip = false;
	return _sieveDataset;
}

void ClusterFilter::onPrepare()
{
	Transformation::onPrepare();

	if (this->progress)
	{
		_progressBinarization = [this](float complete, std::string message)
			{
				return this->progress(complete / 4, message);
			};
		_progressSieve = [this](float complete, std::string message)
			{
				return this->progress(.25f + complete / 2, message);
			};
		_progressFilter = [this](float complete, std::string message)
			{
				return this->progress(.75f + complete / 4, message);
			};
	}
	else
		_progressBinarization = _progressSieve = _progressFilter = nullptr;
}

void ClusterFilter::onExecute()
{
	// Binarization of input data
	SweepLineTransformation<GByte, float> binarization(
		{_sourceDatasets[0]}, _sievePath, 0,
		[](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];
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
		nullptr, gdalProgress, static_cast<void*>(&_progressSieve));

	// Apply sieve filter on input
	SweepLineTransformation<float> filter(std::vector<GDALDataset*>{_sourceDatasets[0], _sieveDataset},
	                                      _targetPath, 0, nullptr, _progressFilter);
	filter.computation = [&filter]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& data = sources[0];
			const Window<float>& sieve = sources[1];

			if (sieve.hasData() && sieve.data() == 255) return data.data();
			else return static_cast<float>(filter.nodataValue);
		};
	filter.nodataValue = nodataValue;
	filter.targetFormat = targetFormat;
	filter.createOptions = createOptions;
	filter.spatialReference = spatialReference;
	filter.execute();
	_targetDataset = filter.target();
}

int ClusterFilter::gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg)
{
	ProgressType* progress = static_cast<ProgressType*>(pProgressArg);
	return (*progress)(static_cast<float>(dfComplete), pszMessage);
}
} // Buildings
} // AHN
