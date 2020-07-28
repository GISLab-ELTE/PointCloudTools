#include <algorithm>

#include <cpl_conv.h>

#include "Helper.h"

namespace CloudTools
{
namespace DEM
{
GDALDataType gdalType(std::string& dataType)
{
	std::transform(dataType.begin(), dataType.end(), dataType.begin(), ::tolower);
	if (dataType == "byte")
		return GDALDataType::GDT_Byte;
	if (dataType == "int16")
		return GDALDataType::GDT_Int16;
	if (dataType == "uint16")
		return GDALDataType::GDT_UInt16;
	if (dataType == "int32")
		return GDALDataType::GDT_Int32;
	if (dataType == "uint32")
		return GDALDataType::GDT_UInt32;
	if (dataType == "float32")
		return GDALDataType::GDT_Float32;
	if (dataType == "float64")
		return GDALDataType::GDT_Float64;

	// Complex types are not supported.
	return GDALDataType::GDT_Unknown;
}

std::string SRSName(const OGRSpatialReference& reference)
{
	const char* authorityName = reference.GetAuthorityName(nullptr);
	const char* authorityCode = reference.GetAuthorityCode(nullptr);

	if (authorityName && authorityCode)
		return std::string(authorityName).append(":").append(authorityCode);
	else
		return std::string();
}

std::string SRSDescription(const OGRSpatialReference& reference)
{
	char *wkt;
	reference.exportToPrettyWkt(&wkt);
	std::string srs = std::string(wkt);
    CPLFree(wkt);

	return srs;
}
} // DEM
} // CloudTools
