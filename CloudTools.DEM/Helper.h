#pragma once

#include <string>
#include <utility>
#include <typeinfo>

#include <boost/functional/hash/hash.hpp>

#include <gdal.h>
#include <ogr_spatialref.h>

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// A point coordinate in a DEM.
/// </summary>
typedef std::pair<int, int> Point;

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
std::string SRSName(const OGRSpatialReference& reference);

/// <summary>
/// Retrieves the pretty-formatted WKT string for a spatial reference system.
/// </summary>
/// <param name="reference">The spatial reference system.</param>
std::string SRSDescription(const OGRSpatialReference& reference);
} // DEM
} // CloudTools

namespace std
{
/// <summary>
/// Represents the default hashing functor for std::pair objects.
/// </summary>
template <class T1, class T2>
struct hash<std::pair<T1, T2>>
{
    std::size_t operator() (const std::pair<T1, T2>& p) const
    {
        std::size_t seed = 0;
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        boost::hash_combine(seed, h1);
        boost::hash_combine(seed, h2);
        return seed;
    }
};
} // std
