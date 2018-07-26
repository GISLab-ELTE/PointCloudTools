#pragma once

#include <string>
#include <typeinfo>

#include <gdal.h>
#include <ogr_spatialref.h>

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Returns the GDAL type for <paramref name="T" />.
/// </summary>
template <typename T>
GDALDataType gdalType()
{
	const std::type_info& typeInfo = typeid(T);
	if (typeInfo == typeid(GByte))
		return GDALDataType::GDT_Byte;
	if (typeInfo == typeid(GUInt16))
		return GDALDataType::GDT_UInt16;
	if (typeInfo == typeid(GInt16))
		return GDALDataType::GDT_Int16;
	if (typeInfo == typeid(GUInt32))
		return GDALDataType::GDT_UInt32;
	if (typeInfo == typeid(GInt32))
		return GDALDataType::GDT_Int32;
	if (typeInfo == typeid(float))
		return GDALDataType::GDT_Float32;
	if (typeInfo == typeid(double))
		return GDALDataType::GDT_Float64;
	
	// Complex types are not supported.
	return GDALDataType::GDT_Unknown;
}

/// <summary>
/// Returns the GDAL type for <paramref name="dataType" />.
/// </summary>
/// <remarks>
/// May change the capitalization of the <paramref name="dataType" /> parameter.
/// </remarks>
GDALDataType gdalType(std::string& dataType);

/// <summary>
/// Retrieves the well known name for a spatial reference system.
/// </summary>
/// <param name="reference">The spatial reference system.</param>
std::string SRSName(const OGRSpatialReference* reference);

/// <summary>
/// Retrieves the pretty-formatted WKT string for a spatial reference system.
/// </summary>
/// <param name="reference">The spatial reference system.</param>
std::string SRSDescription(const OGRSpatialReference* reference);
} // DEM
} // CloudTools
