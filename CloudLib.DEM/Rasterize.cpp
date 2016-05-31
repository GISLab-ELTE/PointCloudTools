#include <cstring>
#include <cmath>
#include <algorithm>
#include <utility>
#include <functional>
#include <stdexcept>

#include <gdal_utils.h>
#include <boost/filesystem.hpp>

#include "Rasterize.h"

#ifdef _MSC_VER
#define strdup _strdup
#endif

namespace fs = boost::filesystem;

namespace CloudLib
{
namespace DEM
{
Rasterize::Rasterize(std::string sourcePath, std::string targetPath,
                     const std::vector<std::string>& layers,
                     ProgressType progress)
	: _sourcePath(sourcePath), _targetPath(targetPath), progress(progress),
	  _targetDataset(nullptr)
{
	// Open source file and retrieve layers
	_sourceDataset = static_cast<GDALDataset*>(GDALOpenEx(_sourcePath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
	if (_sourceDataset == nullptr)
		throw std::runtime_error("Error at opening an input file.");

	if (layers.size() > 0)
	{
		_layers.reserve(layers.size());
		for (const std::string &layerName : layers)
		{
			OGRLayer *layer = _sourceDataset->GetLayerByName(layerName.c_str());
			if (layer == nullptr)
				throw std::invalid_argument("The selected layer does not exist.");
			_layers.push_back(layer);
		}
	}
	else if (_sourceDataset->GetLayerCount() == 1)
		_layers.push_back(_sourceDataset->GetLayer(0));
	else
		throw std::invalid_argument("No layer selected and there are more than 1 layers.");

	// Retrieve source metadata
	_sourceMetadata = VectorMetadata(_layers);
}

Rasterize::~Rasterize()
{
	GDALClose(_sourceDataset);
	if (_targetDataset != nullptr)
		GDALClose(_targetDataset);
}

const VectorMetadata& Rasterize::sourceMetadata() const
{
	return _sourceMetadata;
}

const RasterMetadata& Rasterize::targetMetadata() const
{
	if (!isPrepared())
		throw std::logic_error("The computation is not prepared.");
	return _targetMetadata;
}

void Rasterize::clip(double originX, double originY,
                     int rasterSizeX, int rasterSizeY)
{
	_isClipped = true;
	_targetMetadata.setOriginX(originX);
	_targetMetadata.setOriginY(originY);
	_targetMetadata.setRasterSizeX(rasterSizeX);
	_targetMetadata.setRasterSizeY(rasterSizeY);
}

void Rasterize::onPrepare()
{
	// Calculate the extents for the target raster
	_targetMetadata.setPixelSizeX(pixelSizeX);
	_targetMetadata.setPixelSizeY(pixelSizeY);

	if (!_isClipped)
	{
		_targetMetadata.setOriginX(_sourceMetadata.originX());
		_targetMetadata.setOriginY(_sourceMetadata.originY());
		_targetMetadata.setRasterSizeX(static_cast<int>(std::ceil(_sourceMetadata.extentX() / std::abs(_targetMetadata.pixelSizeX()))));
		_targetMetadata.setRasterSizeY(static_cast<int>(std::ceil(_sourceMetadata.extentY() / std::abs(_targetMetadata.pixelSizeY()))));
	}
	else
	{
		// upperLeft == origin
		double upperLeftX = std::max(_targetMetadata.originX(), _sourceMetadata.originX());
		double upperLeftY = std::min(_targetMetadata.originY(), _sourceMetadata.originY());
		double bottomRightX = std::min(_targetMetadata.originX() + _targetMetadata.extentX(),
		                               _sourceMetadata.originX() + _sourceMetadata.extentX());
		double bottomRightY = std::max(_targetMetadata.originY() - _targetMetadata.extentY(),
		                               _sourceMetadata.originY() - _sourceMetadata.extentY());
		int sizeX = static_cast<int>(std::ceil(bottomRightX - upperLeftX));
		int sizeY = static_cast<int>(std::ceil(upperLeftY - bottomRightY));

		_targetMetadata.setOriginX(upperLeftX);
		_targetMetadata.setOriginY(upperLeftY);
		_targetMetadata.setRasterSizeX(static_cast<int>(std::ceil(sizeX / std::abs(_targetMetadata.pixelSizeX()))));
		_targetMetadata.setRasterSizeY(static_cast<int>(std::ceil(sizeY / std::abs(_targetMetadata.pixelSizeY()))));

		if (sizeX <= 0 || sizeY <= 0)
			throw std::logic_error("The clipping window does not overlap with the data.");
	}

	// Determining spatial reference system
	OGRSpatialReference* reference = nullptr;
	if (spatialReference.length())
	{
		// User defined spatial reference system
		reference = new OGRSpatialReference;
		reference->SetFromUserInput(spatialReference.c_str());
	}
	else if (_sourceMetadata.reference())
	{
		// Input defines spatial reference sytsem
		reference = new OGRSpatialReference(*_sourceMetadata.reference());
	}

	if (reference)
		_targetMetadata.setReference(std::move(reference));
}

void Rasterize::onExecute()
{
	// Create and open the target file
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (fs::exists(_targetPath) &&
		driver->Delete(_targetPath.c_str()) == CE_Failure &&
		!fs::remove(_targetPath))
		throw std::runtime_error("Cannot overwrite previously created output file.");

	// Define the GDALRasterize parameters
	std::vector<char*> params(_layers.size() * 2);
	for (unsigned int i = 0; i < _layers.size(); ++i)
	{
		params[2 * i] = strdup("-l");
		params[2 * i + 1] = strdup(_layers[i]->GetName());
	}
	params.push_back(strdup("-burn"));
	params.push_back(strdup(std::to_string(targetValue).c_str()));
	params.push_back(strdup("-a_nodata"));
	params.push_back(strdup("0"));
	params.push_back(strdup("-ts"));
	params.push_back(strdup(std::to_string(_targetMetadata.rasterSizeX()).c_str()));
	params.push_back(strdup(std::to_string(_targetMetadata.rasterSizeY()).c_str()));
	params.push_back(strdup("-tr"));
	params.push_back(strdup(std::to_string(std::abs(_targetMetadata.pixelSizeX())).c_str()));
	params.push_back(strdup(std::to_string(std::abs(_targetMetadata.pixelSizeY())).c_str()));
	if (_isClipped)
	{
		params.push_back(strdup("-te"));
		params.push_back(strdup(std::to_string(_targetMetadata.originX()).c_str()));
		params.push_back(strdup(std::to_string(_targetMetadata.originY() - _targetMetadata.extentY()).c_str()));
		params.push_back(strdup(std::to_string(_targetMetadata.originX() + _targetMetadata.extentX()).c_str()));
		params.push_back(strdup(std::to_string(_targetMetadata.originY()).c_str()));
	}
	params.push_back(strdup("-ot"));
	params.push_back(strdup("Byte"));
	for (std::string& co : createOptions)
	{
		params.push_back(strdup("-co"));
		params.push_back(strdup(co.c_str()));
	}
	params.push_back(nullptr);
	
	// Execute GDALRasterize
	GDALRasterizeOptions *options = GDALRasterizeOptionsNew(&params[0], nullptr);
	GDALRasterizeOptionsSetProgress(options, gdalProgress, static_cast<void*>(this));
	_targetDataset = static_cast<GDALDataset*>(GDALRasterize(_targetPath.c_str(), nullptr, _sourceDataset, options, nullptr));
	GDALRasterizeOptionsFree(options);

	// Set the spatial reference system
	if (_targetMetadata.reference())
	{
		char *wkt;
		_targetMetadata.reference()->exportToWkt(&wkt);
		_targetDataset->SetProjection(wkt);
		OGRFree(wkt);
	}
}

int Rasterize::gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg)
{
	Rasterize *filter = static_cast<Rasterize*>(pProgressArg);
	return filter->progress(static_cast<float>(dfComplete), std::string());
}
} // DEM
} // CloudLib
