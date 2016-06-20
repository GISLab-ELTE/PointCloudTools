#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

#include <ogr_spatialref.h>
#include <boost/filesystem.hpp>

#include "CalculationBase.h"

namespace fs = boost::filesystem;

namespace CloudLib
{
namespace DEM
{
CalculationBase::CalculationBase(const std::vector<std::string>& sourcePaths,
                                 const std::string& targetPath,
                                 ProgressType progress)
	: _sourcePaths(sourcePaths), _targetPath(targetPath), progress(progress),
	  _sourceOwnership(true), _targetDataset(nullptr)
{
	// Open source files and retrieve metadata
	_sourceDatasets.reserve(sourceCount());
	for (std::string& path : _sourcePaths)
		_sourceDatasets.push_back(static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly)));

	if (sourceCount() == 0)
		throw std::invalid_argument("At least 1 source file must be given.");

	if (std::any_of(_sourceDatasets.begin(), _sourceDatasets.end(),
		[](GDALDataset* dataset)
	{
		return dataset == nullptr;
	}))
		throw std::runtime_error("Error at opening a source file.");

	_sourceMetadata.reserve(sourceCount());
	for (GDALDataset* dataset : _sourceDatasets)
		_sourceMetadata.emplace_back(dataset);
}

CalculationBase::CalculationBase(const std::vector<GDALDataset*>& sourceDatasets,
                                 const std::string& targetPath,
                                 ProgressType progress)
	: _sourceDatasets(sourceDatasets), _targetPath(targetPath), progress(progress),
	  _sourceOwnership(false), _targetDataset(nullptr)
{
	// Check source datasets and retrieve metadata
	if (sourceCount() == 0)
		throw std::invalid_argument("At least 1 source file must be given.");

	if (std::any_of(_sourceDatasets.begin(), _sourceDatasets.end(),
		[](GDALDataset* dataset)
	{
		return dataset == nullptr;
	}))
		throw std::invalid_argument("Invalid source file.");

	_sourceMetadata.reserve(sourceCount());
	for (GDALDataset* dataset : _sourceDatasets)
		_sourceMetadata.emplace_back(dataset);
}

CalculationBase::~CalculationBase()
{
	if (_sourceOwnership)
		for (GDALDataset* dataset : _sourceDatasets)
			GDALClose(dataset);
	if (_targetOwnerShip && _targetDataset != nullptr)
		GDALClose(_targetDataset);
}

const RasterMetadata& CalculationBase::sourceMetadata(unsigned int index) const
{
	return _sourceMetadata.at(index);
}

const RasterMetadata& CalculationBase::sourceMetadata(const std::string& file) const
{
	auto iterator = std::find(_sourcePaths.begin(), _sourcePaths.end(), file);
	if (iterator != _sourcePaths.end())
		return _sourceMetadata.at(iterator - _sourcePaths.begin());
	else
		throw std::out_of_range("File not found in the sources.");
}

const RasterMetadata& CalculationBase::targetMetadata() const
{
	if (!isPrepared())
		throw std::logic_error("The computation is not prepared.");
	return _targetMetadata;
}

const GDALDataset* CalculationBase::target() const
{
	if (!isExecuted())
		throw std::logic_error("The computation is not executed.");
	_targetOwnerShip = false;
	return _targetDataset;
}

void CalculationBase::onPrepare()
{
	// Verify matching pixel sizes
	std::vector<double> pixelSizes(_sourceMetadata.size());
	std::transform(_sourceMetadata.begin(), _sourceMetadata.end(), pixelSizes.begin(),
		[](RasterMetadata& metadata)
	{
		return metadata.pixelSizeX();
	});
	if (std::min_element(pixelSizes.begin(), pixelSizes.end()) !=
		std::max_element(pixelSizes.begin(), pixelSizes.end()))
		throw std::logic_error("Horizontal pixel sizes differ.");

	std::transform(_sourceMetadata.begin(), _sourceMetadata.end(), pixelSizes.begin(),
		[](RasterMetadata& metadata)
	{
		return metadata.pixelSizeY();
	});
	if (std::min_element(pixelSizes.begin(), pixelSizes.end()) !=
		std::max_element(pixelSizes.begin(), pixelSizes.end()))
		throw std::logic_error("Vertical pixel sizes differ.");

	// Determining spatial reference system
	OGRSpatialReference* reference = nullptr;
	if (spatialReference.length())
	{
		// User defined spatial reference system
		reference = new OGRSpatialReference;
		reference->SetFromUserInput(spatialReference.c_str());
	}
	else
	{
		// Verify matching spatial reference systems
		std::vector<OGRSpatialReference*> references(_sourceMetadata.size());
		std::transform(_sourceMetadata.begin(), _sourceMetadata.end(), references.begin(),
			[](RasterMetadata& metadata)
		{
			return metadata.reference();
		});
		references.erase(std::remove_if(
			references.begin(), references.end(),
			[](OGRSpatialReference* reference)
		{
			return reference == nullptr ||
				reference->Validate() != OGRERR_NONE;
		}), references.end());

		for (unsigned int i = 1; i < references.size(); ++i)
			if (!references[i - 1]->IsSame(references[i]))
				throw std::logic_error("Spatial reference systems for the sources differ.");

		// Set the reference system
		if (references.size())
			reference = new OGRSpatialReference(*references[0]);
		else
			throw std::logic_error("No spatial reference system in the source files and none are given manually.");
	}

	// Calculate target metadata
	_targetMetadata.setOriginX(
		std::min_element(_sourceMetadata.begin(), _sourceMetadata.end(),
		[](RasterMetadata& a, RasterMetadata& b)
	{
		return a.originX() < b.originX();
	})->originX());

	_targetMetadata.setOriginY(
		std::max_element(_sourceMetadata.begin(), _sourceMetadata.end(),
		[](RasterMetadata& a, RasterMetadata& b)
	{
		return a.originY() < b.originY();
	})->originY());

	std::vector<double> endPositions(_sourceMetadata.size());
	std::transform(_sourceMetadata.begin(), _sourceMetadata.end(), endPositions.begin(),
		[](RasterMetadata& metadata)
	{
		return metadata.originX() + metadata.extentX();
	});
	double extentX = -_targetMetadata.originX() +
		*std::max_element(endPositions.begin(), endPositions.end());

	std::transform(_sourceMetadata.begin(), _sourceMetadata.end(), endPositions.begin(),
		[](RasterMetadata& metadata)
	{
		return metadata.originY() - metadata.extentY();
	});
	double extentY = _targetMetadata.originY() -
		*std::min_element(endPositions.begin(), endPositions.end());

	_targetMetadata.setPixelSizeX(_sourceMetadata[0].pixelSizeX());
	_targetMetadata.setPixelSizeY(_sourceMetadata[0].pixelSizeY());
	_targetMetadata.setRasterSizeX(static_cast<int>(std::abs(extentX / _targetMetadata.pixelSizeX())));
	_targetMetadata.setRasterSizeY(static_cast<int>(std::abs(extentY / _targetMetadata.pixelSizeY())));
	_targetMetadata.setReference(std::move(reference));
}
} // DEM
} // CloudLib
