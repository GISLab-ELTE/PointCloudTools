#pragma once

#include <string>

#include <boost/filesystem.hpp>
#include <gdal_priv.h>

namespace fs = boost::filesystem;

namespace CloudTools
{
namespace IO
{
/// <summary>
/// Represents a result object.
/// </summary>
struct Result
{
protected:
	/// <summary>
	/// File path.
	/// </summary>
	/// <remarks>
	/// Empty for GDAL memory objects, starts with <c>"/vsimem/"</c> for virtual files.
	/// </remarks>
	fs::path _path;

public:
	/// <summary>
	/// Dataset.
	/// </summary>
	GDALDataset* dataset;

protected:
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit Result(const std::string& path, GDALDataset* dataset = nullptr)
		: _path(path), dataset(dataset)
	{ }

	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit Result(const fs::path& path, GDALDataset* dataset = nullptr)
		: _path(path), dataset(dataset)
	{ }

public:
	virtual ~Result();
	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;

	Result(Result&& other) noexcept;
	Result& operator=(Result&& other) noexcept;

	/// <summary>
	/// Gets the path.
	/// </summary>
	std::string path() const { return _path.string(); }
};

/// <summary>
/// Represents a permanent file result.
/// </summary>
struct PermanentFileResult : Result
{
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit PermanentFileResult(const std::string& path, GDALDataset* dataset = nullptr)
		: Result(path, dataset)
	{ }

	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit PermanentFileResult(const fs::path& path, GDALDataset* dataset = nullptr)
		: Result(path, dataset)
	{ }

	PermanentFileResult(const PermanentFileResult&) = delete;
	PermanentFileResult& operator=(const PermanentFileResult&) = delete;

	PermanentFileResult(PermanentFileResult&& other) noexcept
		: Result(std::move(other))
	{ }

	PermanentFileResult& operator=(PermanentFileResult&& other) noexcept
	{
		*this = std::move(other);
		return *this;
	}
};

/// <summary>
/// Represents a temporary file result.
/// </summary>
struct TemporaryFileResult : PermanentFileResult
{
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit TemporaryFileResult(const std::string& path, GDALDataset* dataset = nullptr)
		: PermanentFileResult(path, dataset)
	{ }

	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit TemporaryFileResult(const fs::path& path, GDALDataset* dataset = nullptr)
		: PermanentFileResult(path, dataset)
	{ }

	~TemporaryFileResult();
	TemporaryFileResult(const TemporaryFileResult&) = delete;
	TemporaryFileResult& operator=(const TemporaryFileResult&) = delete;

	TemporaryFileResult(TemporaryFileResult&& other) noexcept
		: PermanentFileResult(std::move(other))
	{ }

	TemporaryFileResult& operator=(TemporaryFileResult&& other) noexcept
	{
		*this = std::move(other);
		return *this;
	}
};

/// <summary>
/// Represents a virtual file result.
/// </summary>
struct VirtualResult : Result
{
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <remarks>
	/// The <paramref name="path"/> must start with <c>"/vsimem/"</c>, otherwise added.
	/// </remarks>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit VirtualResult(const std::string& path, GDALDataset* dataset = nullptr);

	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <remarks>
	/// The <paramref name="path"/> must start with <c>"/vsimem/"</c>, otherwise added.
	/// </remarks>
	/// <param name="path">The path.</param>
	/// <param name="dataset">The dataset.</param>
	explicit VirtualResult(const fs::path& path, GDALDataset* dataset = nullptr);

	~VirtualResult();
	VirtualResult(const VirtualResult&) = delete;
	VirtualResult& operator=(const VirtualResult&) = delete;

	VirtualResult(VirtualResult&& other) noexcept
		: Result(std::move(other))
	{ }

	VirtualResult& operator=(VirtualResult&& other) noexcept
	{
		*this = std::move(other);
		return *this;
	}
};

/// <summary>
/// Represents a GDAL memory object result.
/// </summary>
struct MemoryResult : Result
{
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="dataset">The dataset.</param>
	explicit MemoryResult(GDALDataset* dataset = nullptr)
		: Result(std::string(), dataset)
	{
	}

	MemoryResult(const MemoryResult&) = delete;
	MemoryResult& operator=(const MemoryResult&) = delete;

	MemoryResult(MemoryResult&& other) noexcept
		: Result(std::move(other))
	{ }

	MemoryResult& operator=(MemoryResult&& other) noexcept
	{
		*this = std::move(other);
		return *this;
	}
};
} // IO
} // CloudTools
