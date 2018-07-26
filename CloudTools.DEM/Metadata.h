#pragma once

#include <array>
#include <vector>
#include <iosfwd>
#include <cmath>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a metadata collection of a DEM.
/// </summary>
class Metadata
{
protected:
	Metadata() { };

public:
	Metadata(const Metadata&) = delete;
	Metadata& operator= (const Metadata&) = delete;
	virtual ~Metadata() {}

	virtual double originX() const = 0;
	virtual double originY() const = 0;
	virtual double extentX() const = 0;
	virtual double extentY() const = 0;
	virtual const OGRSpatialReference* reference() const = 0;
};

/// <summary>
/// Represents a metadata collection of vector file.
/// </summary>
class VectorMetadata : public Metadata
{
protected:
	double _originX;
	double _originY;
	double _extentX;
	double _extentY;
	OGRSpatialReference* _reference;

public:
	VectorMetadata() : _originX(0), _originY(0),
		_extentX(0), _extentY(0),
		_reference(nullptr)
	{ }
	VectorMetadata(GDALDataset* dataset, const std::vector<std::string>& layers);
	VectorMetadata(const std::vector<OGRLayer*>& layers);
	~VectorMetadata();

	VectorMetadata(const VectorMetadata& other);
	VectorMetadata& operator= (const VectorMetadata& other);

	bool operator==(const VectorMetadata &other) const;
	bool operator!=(const VectorMetadata &other) const;

#pragma region Getters

	double originX() const override { return _originX; }
	double originY() const override { return _originY; }
	double extentX() const override { return _extentX; }
	double extentY() const override { return _extentY; }
	const OGRSpatialReference* reference() const override {	return _reference; }
	virtual OGRSpatialReference* reference() { return _reference; }

#pragma endregion

#pragma region Setters

	virtual void setOriginX(double originX) { _originX = originX; }
	virtual void setOriginY(double originY) { _originY = originY; }
	virtual void setExtentX(double extentX) { _extentX = extentX; }
	virtual void setExtentY(double extentY) { _extentY = extentY; }
	virtual void setReference(const OGRSpatialReference &reference)
	{
		_reference = new OGRSpatialReference(reference);
	}
	virtual void setReference(OGRSpatialReference* &&reference)
	{
		_reference = reference;
		reference = nullptr;
	}

#pragma endregion

private:
	void load(const std::vector<OGRLayer*>& layers);
};

/// <summary>
/// Represents a metadata collection of a raster file.
/// </summary>
class RasterMetadata : public Metadata
{
protected:
	double _originX;
	double _originY;
	int _rasterSizeX;
	int _rasterSizeY;
	double _pixelSizeX;
	double _pixelSizeY;
	double _extentX;
	double _extentY;
	OGRSpatialReference* _reference;

public:
	RasterMetadata() : _originX(0), _originY(0),
	                   _rasterSizeX(0), _rasterSizeY(0),
	                   _pixelSizeX(0), _pixelSizeY(0),
	                   _extentX(0), _extentY(0),
					   _reference(nullptr)
	{ }
	RasterMetadata(GDALDataset* dataset);
	~RasterMetadata();

	RasterMetadata(const RasterMetadata& other);
	RasterMetadata& operator= (const RasterMetadata& other);

	bool operator==(const RasterMetadata &other) const;
	bool operator!=(const RasterMetadata &other) const;

#pragma region Getters

	double originX() const override { return _originX; }
	double originY() const override { return _originY; }
	virtual int rasterSizeX() const { return _rasterSizeX; }
	virtual int rasterSizeY() const { return _rasterSizeY; }
	virtual double pixelSizeX() const { return _pixelSizeX; }
	virtual double pixelSizeY() const { return _pixelSizeY; }
	double extentX() const override { return _extentX; }
	double extentY() const override { return _extentY; }
	const OGRSpatialReference* reference() const override { return _reference; }
	virtual OGRSpatialReference* reference() { return _reference; }

#pragma endregion

#pragma region Setters

	virtual void setOriginX(double originX) { _originX = originX; }
	virtual void setOriginY(double originY) { _originY = originY; }
	virtual void setRasterSizeX(int rasterSizeX)
	{
		_rasterSizeX = rasterSizeX;
		_extentX = std::abs(_rasterSizeX * _pixelSizeX);
	}

	virtual void setRasterSizeY(int rasterSizeY)
	{
		_rasterSizeY = rasterSizeY;
		_extentY = std::abs(_rasterSizeY * _pixelSizeY);
	}
	virtual void setPixelSizeX(double pixelSizeX)
	{
		_pixelSizeX = pixelSizeX;
		_extentX = std::abs(_rasterSizeX * _pixelSizeX);
	}

	virtual void setPixelSizeY(double pixelSizeY)
	{
		_pixelSizeY = pixelSizeY;
		_extentY = std::abs(_rasterSizeY * _pixelSizeY);
	}
	virtual void setExtentX(double extentX)
	{
		setRasterSizeX(static_cast<int>(std::ceil(extentX / _pixelSizeX)));
	}
	virtual void setExtentY(double extentY)
	{
		setRasterSizeX(static_cast<int>(std::ceil(extentY / _pixelSizeY)));
	}
	virtual void setReference(const OGRSpatialReference &reference)
	{
		_reference = new OGRSpatialReference(reference);
	}
	virtual void setReference(OGRSpatialReference* &&reference)
	{
		_reference = reference;
		reference = nullptr;
	}

#pragma endregion

	/// <summary>
	/// Returns the georeferencing transform array (using format of GDAL).
	/// </summary>
	std::array<double, 6> geoTransform() const;

	/// <summary>
	/// Loads the metadata from the georeferencing transform array (using format of GDAL).
	/// </summary>
	void setGeoTransform(const std::array<double, 6>& geoTransform);

	/// <summary>
	/// Loads the metadata from the georeferencing transform (using format of GDAL).
	/// </summary>
	void setGeoTransform(const double* geoTransform);
};

std::ostream& operator<<(std::ostream& out, const Metadata& metadata);
std::ostream& operator<<(std::ostream& out, const RasterMetadata& metadata);
} // DEM
} // CloudTools
