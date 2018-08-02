#include <cstring>
#include <cmath>
#include <algorithm>
#include <utility>
#include <functional>
#include <stdexcept>

#include <gdal_utils.h>
#include <boost/filesystem.hpp>

#include "Rasterize.h"

namespace fs = boost::filesystem;

namespace CloudTools
{
namespace DEM
{
Rasterize::Rasterize(const std::string& sourcePath, const std::string& targetPath,
                     const std::vector<std::string>& layers,
                     ProgressType progress)
	: _sourcePath(sourcePath), _targetPath(targetPath), progress(progress),
	  _sourceOwnership(true), _targetDataset(nullptr)
{
	// Open source file and retrieve layers
	_sourceDataset = static_cast<GDALDataset*>(GDALOpenEx(_sourcePath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
	if (_sourceDataset == nullptr)
		throw std::runtime_error("Error at opening the source file.");

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

Rasterize::Rasterize(GDALDataset* sourceDataset, const std::string& targetPath,
                     const std::vector<std::string>& layers,
                     ProgressType progress)
	: _sourceDataset(sourceDataset), _targetPath(targetPath), progress(progress),
	  _sourceOwnership(false), _targetDataset(nullptr)
{
	// Check source dataset and retrieve layers
	if (_sourceDataset == nullptr)
		throw std::invalid_argument("Invalid source file.");

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
	if (_sourceOwnership)
		GDALClose(_sourceDataset);
	if (_targetOwnerShip && _targetDataset != nullptr)
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

GDALDataset* Rasterize::target()
{
	if (!isExecuted())
		throw std::logic_error("The computation is not executed.");
	_targetOwnerShip = false;
	return _targetDataset;
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
	OGRSpatialReference reference;
	if (spatialReference.length())
	{
		// User defined spatial reference system
		reference.SetFromUserInput(spatialReference.c_str());
	}
	else if (_sourceMetadata.reference().Validate() == OGRERR_NONE)
	{
		// Input defines spatial reference system
		reference = _sourceMetadata.reference();
	}

	if (reference.Validate() == OGRERR_NONE)
		_targetMetadata.setReference(reference);

	// Check the existence of the target field and determine data type
	if (!targetField.empty())
	{
		OGRFieldType sourceType;
		bool validField = std::any_of(_layers.begin(), _layers.end(),
			[this, &sourceType](OGRLayer* layer)
		{
			int index = layer->FindFieldIndex(targetField.c_str(), false);
			if (index < 0) return false;

			OGRFeatureDefn* feature = layer->GetLayerDefn();
			OGRFieldDefn* field = feature->GetFieldDefn(index);
			sourceType = field->GetType();
			return true;
		});
		if (!validField)
			throw std::runtime_error("None of the given layers contain the target field.");

		// Check support for field data type
		if (targetType == GDALDataType::GDT_Unknown)
		{
			switch (sourceType)
			{
			case OGRFieldType::OFTBinary:
				targetType = GDALDataType::GDT_Byte;
				break;
			case OGRFieldType::OFTInteger:
				targetType = GDALDataType::GDT_Int32;
				break;
			case OGRFieldType::OFTReal:
				targetType = GDALDataType::GDT_Float64;
				break;
			default:
				throw std::runtime_error("Field data type not supported.");
			}
		}
	}
	else if (targetType == GDALDataType::GDT_Unknown)
		targetType = GDALDataType::GDT_Byte;
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
	char **params = nullptr;
	for (unsigned int i = 0; i < _layers.size(); ++i)
	{
		params = CSLAddString(params, "-l");
		params = CSLAddString(params, _layers[i]->GetName());
	}
	if (targetField.empty())
	{
		params = CSLAddString(params, "-burn");
		params = CSLAddString(params, std::to_string(targetValue).c_str());
	}
	else
	{
		params = CSLAddString(params, "-a");
		params = CSLAddString(params, targetField.c_str());
	}
	params = CSLAddString(params, "-a_nodata");
	params = CSLAddString(params, std::to_string(nodataValue).c_str());
	params = CSLAddString(params, "-ts");
	params = CSLAddString(params, std::to_string(_targetMetadata.rasterSizeX()).c_str());
	params = CSLAddString(params, std::to_string(_targetMetadata.rasterSizeY()).c_str());
	params = CSLAddString(params, "-tr");
	params = CSLAddString(params, std::to_string(std::abs(_targetMetadata.pixelSizeX())).c_str());
	params = CSLAddString(params, std::to_string(std::abs(_targetMetadata.pixelSizeY())).c_str());
	if (_isClipped)
	{
		params = CSLAddString(params, "-te");
		params = CSLAddString(params, std::to_string(_targetMetadata.originX()).c_str());
		params = CSLAddString(params, std::to_string(_targetMetadata.originY() - _targetMetadata.extentY()).c_str());
		params = CSLAddString(params, std::to_string(_targetMetadata.originX() + _targetMetadata.extentX()).c_str());
		params = CSLAddString(params, std::to_string(_targetMetadata.originY()).c_str());
	}
	params = CSLAddString(params, "-ot");
	switch(targetType)
	{
	case GDALDataType::GDT_Byte:
		params = CSLAddString(params, "Byte");
		break;
	case GDALDataType::GDT_Int16:
		params = CSLAddString(params, "Int16");
		break;
	case GDALDataType::GDT_Int32:
		params = CSLAddString(params, "Int32");
		break;
	case GDALDataType::GDT_UInt16:
		params = CSLAddString(params, "UInt16");
		break;
	case GDALDataType::GDT_UInt32:
		params = CSLAddString(params, "UInt32");
		break;
	case GDALDataType::GDT_Float32:
		params = CSLAddString(params, "Float32");
		break;
	case GDALDataType::GDT_Float64:
		params = CSLAddString(params, "Float64");
		break;
	default:
		throw std::runtime_error("Complex number types are not supported.");
	}
	for (auto& co : createOptions)
	{
		params = CSLAddString(params, "-co");
		params = CSLSetNameValue(params, co.first.c_str(), co.second.c_str());
	}
	params = CSLAddString(params, "-of");
	params = CSLAddString(params, targetFormat.c_str());
	
	// Execute GDALRasterize
	GDALRasterizeOptions *options = GDALRasterizeOptionsNew(params, nullptr);
	GDALRasterizeOptionsSetProgress(options, gdalProgress, static_cast<void*>(this));
	_targetDataset = static_cast<GDALDataset*>(GDALRasterize(_targetPath.c_str(), nullptr, _sourceDataset, options, nullptr));
	GDALRasterizeOptionsFree(options);
	CSLDestroy(params);

	// Set the spatial reference system
	if (_targetMetadata.reference().Validate() == OGRERR_NONE)
	{
		char *wkt;
		_targetMetadata.reference().exportToWkt(&wkt);
		_targetDataset->SetProjection(wkt);
		OGRFree(wkt);
	}
}

int Rasterize::gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg)
{
	Rasterize *filter = static_cast<Rasterize*>(pProgressArg);
	if (!filter->progress) return true;
	return filter->progress(static_cast<float>(dfComplete), std::string());
}
} // DEM
} // CloudTools
