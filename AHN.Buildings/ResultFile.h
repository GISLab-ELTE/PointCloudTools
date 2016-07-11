#pragma once

#include <string>

#include <boost/filesystem.hpp>
#include <gdal_priv.h>

namespace fs = boost::filesystem;

namespace AHN
{	
/// <summary>
/// Represents a result object (physical file, virtual file, memory object).
/// </summary>
struct ResultFile final
{	
	/// <summary>
	/// File path.
	/// </summary>
	/// <remarks>
	/// Empty for GDAL memory objects, starts with "/vsimem/" for virtual files.
	/// </remakrs>
	fs::path path;	
	/// <summary>
	/// Dataset.
	/// </summary>
	GDALDataset* dataset;	
	/// <summary>
	/// Permanent physical result files are not removed on destruction.
	/// </summary>
	bool isPermanent;
	
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	/// <param name="isPermanent"><c>true</c> if result is permanent, otherwise <c>false</c>.</param>
	ResultFile(fs::path path, GDALDataset* dataset, bool isPermanent = false)
		: path(path), dataset(dataset), isPermanent(isPermanent)
	{
		isPhysical();
	}
	
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="isPermanent"><c>true</c> if result is permanent, otherwise <c>false</c>.</param>
	ResultFile(fs::path path, bool isPermanent = false)
		: path(path), dataset(nullptr), isPermanent(isPermanent)
	{
		isPhysical();
	}
	
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="isPermanent"><c>true</c> if result is permanent, otherwise <c>false</c>.</param>
	ResultFile(bool isPermanent = false)
		: path(std::string()), dataset(nullptr), isPermanent(isPermanent)
	{
		isPhysical();
	}

	~ResultFile();
	ResultFile(const ResultFile&) = delete;
	ResultFile& operator=(const ResultFile&) = delete;

	ResultFile(ResultFile&& other);
	ResultFile& operator=(ResultFile&& other);

	/// <summary>
	/// Determines whether this instance is physical file on the disk.
	/// </summary>
	bool isPhysical() const;

	/// <summary>
	/// Determines whether this instance is VSI file in the memory.
	/// </summary>
	bool isVirtual() const;

	/// <summary>
	/// Determines whether this instance is a GDAL memory object.
	/// </summary>
	bool isInMemory() const;
};
} // AHN
