#include <stdexcept>
#include <ostream>
#include <algorithm>
#include <stdexcept>

#include <ogrsf_frmts.h>

#include "Metadata.h"
#include "Helper.h"

namespace CloudTools
{
namespace DEM
{
#pragma region VectorMetadata

VectorMetadata::VectorMetadata(GDALDataset* dataset, const std::vector<std::string>& layerNames)
{
	std::vector<OGRLayer*> layers(layerNames.size());
	if (!layerNames.empty())
	{
		for (const std::string &layerName : layerNames)
		{
			OGRLayer *layer = dataset->GetLayerByName(layerName.c_str());
			if (layer == nullptr)
				throw std::invalid_argument("The selected layer does not exist.");
			layers.push_back(layer);
		}
	}
	else if (dataset->GetLayerCount() == 1)
		layers.push_back(dataset->GetLayer(0));
	else
		throw std::invalid_argument("No layer selected and there are more than 1 layers.");

	load(layers);
}

VectorMetadata::VectorMetadata(const std::vector<OGRLayer*>& layers)
{
	load(layers);
}

void VectorMetadata::load(const std::vector<OGRLayer*>& layers)
{
	// Retrieving spatial positions
	std::vector<OGREnvelope*> extents(layers.size());
	std::transform(layers.begin(), layers.end(), extents.begin(),
		[](OGRLayer *layer)
	{
		OGREnvelope *extent = new OGREnvelope;
		if (layer->GetExtent(extent, true) != OGRERR_NONE)
			throw std::logic_error("Extent unknown for an input layer.");
		return extent;
	});

	double minX = std::floor((*std::min_element(extents.begin(), extents.end(),
		[](const OGREnvelope* a, const OGREnvelope* b)
	{
		return a->MinX < b->MinX;
	}))->MinX);

	double maxX = std::ceil((*std::max_element(extents.begin(), extents.end(),
		[](const OGREnvelope* a, const OGREnvelope* b)
	{
		return a->MaxX < b->MaxX;
	}))->MaxX);

	double minY = std::floor((*std::min_element(extents.begin(), extents.end(),
		[](const OGREnvelope* a, const OGREnvelope* b)
	{
		return a->MinY < b->MinY;
	}))->MinY);

	double maxY = std::ceil((*std::max_element(extents.begin(), extents.end(),
		[](const OGREnvelope* a, const OGREnvelope* b)
	{
		return a->MaxY < b->MaxY;
	}))->MaxY);

	_originX = minX;
	_originY = maxY;
	_extentX = maxX - minX;
	_extentY = maxY - minY;
	std::for_each(extents.begin(), extents.end(), [](OGREnvelope* extent) { delete extent; });

	// Retrieving spatial reference system
	std::vector<OGRSpatialReference*> references(layers.size());
	std::transform(layers.begin(), layers.end(), references.begin(),
		[](OGRLayer* layer)
	{
		OGRSpatialReference *reference = layer->GetSpatialRef();
		if (reference == nullptr)
		{
			OGRFeature *feature = layer->GetNextFeature();
			if (feature != nullptr)
			{
				OGRGeometry *geometry = feature->GetGeometryRef();
				reference = new OGRSpatialReference(*geometry->getSpatialReference());
				OGRFeature::DestroyFeature(feature);
			}
		}
		return reference;
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
			throw std::logic_error("Spatial reference systems differ for the input layers.");

	if (!references.empty())
		_reference = *references[0];
	else
		_reference = nullptr;
}

VectorMetadata::VectorMetadata(const VectorMetadata& other)
	: _originX(other._originX), _originY(other._originY),
	_extentX(other._extentX), _extentY(other._extentY),
	_reference(other._reference)
{ }

VectorMetadata& VectorMetadata::operator= (const VectorMetadata& other)
{
	if (this == &other)
		return *this;

	_originX = other._originX;
	_originY = other._originY;
	_extentX = other._extentX;
	_extentY = other._extentY;
	_reference = other._reference;
	return *this;
}

inline bool VectorMetadata::operator==(const VectorMetadata &other) const
{
	return
		_originX == other._originX &&
		_originY == other._originY &&
		_extentX == other._extentX &&
		_extentY == other._extentY &&
		_reference.IsSame(&other._reference);
}

inline bool VectorMetadata::operator!=(const VectorMetadata &other) const
{
	return !(*this == other);
}

#pragma endregion

#pragma region RasterMetadata

RasterMetadata::RasterMetadata(GDALDataset* dataset)
{
	// Retrieving spatial positions
	_rasterSizeX = dataset->GetRasterXSize();
	_rasterSizeY = dataset->GetRasterYSize();

	double geoTransform[6];
	if (dataset->GetGeoTransform(geoTransform) == CE_Failure)
		throw std::logic_error("Error at retrieving geographical transformation.");
	setGeoTransform(geoTransform);

	// Retrieving spatial reference system
	_reference = OGRSpatialReference(dataset->GetProjectionRef());
}

RasterMetadata::RasterMetadata(const RasterMetadata& other)
	: _originX(other._originX), _originY(other._originY),
	  _rasterSizeX(other._rasterSizeX), _rasterSizeY(other._rasterSizeY),
	  _pixelSizeX(other._pixelSizeX), _pixelSizeY(other._pixelSizeY),
	  _extentX(other._extentX), _extentY(other._extentY),
	  _reference(other._reference)
{ }

RasterMetadata& RasterMetadata::operator= (const RasterMetadata& other)
{
	if (this == &other)
		return *this;

	_originX = other._originX;
	_originY = other._originY;
	_rasterSizeX = other._rasterSizeX;
	_rasterSizeY = other._rasterSizeY;
	_pixelSizeX = other._pixelSizeX;
	_pixelSizeY = other._pixelSizeY;
	_extentX = other._extentX;
	_extentY = other._extentY;
	_reference = other._reference;
	return *this;
}

inline bool RasterMetadata::operator==(const RasterMetadata &other) const
{
	return
		_originX == other._originX &&
		_originY == other._originY &&
		_rasterSizeX == other._rasterSizeX &&
		_rasterSizeY == other._rasterSizeY &&
		_pixelSizeX == other._pixelSizeX &&
		_pixelSizeY == other._pixelSizeY &&
		_extentX == other._extentX &&
		_extentY == other._extentY &&
		_reference.IsSame(&other._reference);
}

inline bool RasterMetadata::operator!=(const RasterMetadata &other) const
{
	return !(*this == other);
}

std::array<double, 6> RasterMetadata::geoTransform() const
{
	std::array<double, 6> geoTransform =
	{
		_originX,
		_pixelSizeX,
		0,
		_originY,
		0,
		_pixelSizeY
	};
	return geoTransform;
}

void RasterMetadata::setGeoTransform(const std::array<double, 6>& geoTransform)
{
	setGeoTransform(&geoTransform[0]);
}

void RasterMetadata::setGeoTransform(const double* geoTransform)
{
	_originX = geoTransform[0];
	_originY = geoTransform[3];

	_pixelSizeX = geoTransform[1];
	_pixelSizeY = geoTransform[5];

	_extentX = std::abs(_rasterSizeX * _pixelSizeX);
	_extentY = std::abs(_rasterSizeY * _pixelSizeY);
}

#pragma endregion

#pragma region Output operations

std::ostream& operator<<(std::ostream& out, const Metadata& metadata)
{
	out << "Origin: \t" << metadata.originX() << " x " << metadata.originY() << std::endl;
	out << "Extent: \t" << metadata.extentX() << " x " << metadata.extentY() << std::endl;
	out << "Reference: \t";

	std::string srs = SRSName(metadata.reference());
	if (!srs.length())
	{
		srs = SRSDescription(metadata.reference());
		if (srs.length()) out << "\n";
	}
	if (!srs.length())
		srs = "none";
	out << srs << std::endl;

	return out;
}

std::ostream& operator<<(std::ostream& out, const RasterMetadata& metadata)
{
	out << "Origin: \t" << metadata.originX() << " x " << metadata.originY() << std::endl;
	out << "Raster size: \t" << metadata.rasterSizeX() << " x " << metadata.rasterSizeY() << std::endl;
	out << "Pixel size: \t" << metadata.pixelSizeX() << " x " << metadata.pixelSizeY() << std::endl;
	out << "Extent: \t" << metadata.extentX() << " x " << metadata.extentY() << std::endl;
	out << "Reference: \t";

	std::string srs = SRSName(metadata.reference());
	if (!srs.length())
	{
		srs = SRSDescription(metadata.reference());
		if (srs.length()) out << "\n";
	}
	if (!srs.length())
		srs = "none";
	out << srs << std::endl;

	return out;
}

#pragma endregion
} // DEM
} // CloudTools
