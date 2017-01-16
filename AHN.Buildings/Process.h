#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include <CloudLib.DEM/Operation.h>
#include <CloudLib.DEM/Transformation.h>
#include "Result.h"

namespace fs = boost::filesystem;

namespace AHN
{
namespace Buildings
{
/// <summary>
/// The AHN Building Filter operation.
/// </summary>
class Process : public CloudLib::DEM::Operation
{
public:
	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

protected:
	/// <summary>
	/// Unique identifier, in most cases the name of the tile to process.
	/// </summary>
	std::string _id;

	GDALDataset *_ahn2SurfaceDataset,
	            *_ahn2TerrainDataset,
	            *_ahn3SurfaceDataset,
	            *_ahn3TerrainDataset;

	/// <summary>
	/// Internal progress reporter piped to override message.
	/// </summary>
	ProgressType _progress;
	/// <summary>
	/// The current progress message to report.
	/// </summary>
	std::string _progressMessage;

private:
	std::multimap<std::string, Result*> _results;

public:
	~Process();

protected:	
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="id">Unique identifier, in most cases the name of the tile to process.</param>
	explicit Process(const std::string& id)
		: _id(id),
		  _ahn2SurfaceDataset(nullptr), _ahn2TerrainDataset(nullptr),
		  _ahn3SurfaceDataset(nullptr), _ahn3TerrainDataset(nullptr)
	{
		if (id.empty())
			throw std::invalid_argument("The process identifier must not be empty.");
	}

	/// <summary>
	/// Verifies the configuration.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Produces the output file(s).
	/// </summary>
	void onExecute() override;

	/// <summary>
	/// Gets the specified result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="index">The index of the result with the same name.</param>
	Result& result(const std::string& name, std::size_t index = 0);
	
	/// <summary>
	/// Creates and inserts new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>The index of the result with the same name.</returns>
	std::size_t newResult(const std::string& name, bool isFinal = false);
	
	/// <summary>
	/// Creates a new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>New result on the heap.</returns>
	virtual Result* createResult(const std::string& name, bool isFinal = false) = 0;
	
	/// <summary>
	/// Deletes the specified result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="index">The index of the result with the same name.</param>
	void deleteResult(const std::string& name, std::size_t index = 0);

	/// <summary>
	/// Configures the output format options for the given transformation.
	/// </summary>
	/// <remarks>
	/// The targets of these transformations are never final.
	/// </remarks>
	virtual void configure(CloudLib::DEM::Transformation& transformation) const = 0;

	/// <summary>
	/// Routes the C-style GDAL progress reports to the defined reporter.
	/// </summary>
	static int CPL_STDCALL gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg);
};

/// <summary>
/// The AHN Building Filter operation with intermediate results stored in the memory.
/// </summary>
class InMemoryProcess : public Process
{
public:
	/// <summary>
	/// Map file for color relief.
	/// </summary>
	/// <remarks>
	/// No color relief will be applied when left empty.
	/// </remarks>
	/// <seealso href="http://www.gdal.org/gdaldem.html"/>
	std::string colorFile;

protected:	
	/// <summary>
	/// The output directory path.
	/// </summary>
	std::string _outputPath;

public:	
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="id">Unique identifier, in most cases the name of the tile to process.</param>
	/// <param name="ahn2SurfacePath">The AHN-2 surface dataset path.</param>
	/// <param name="ahn3SurfacePath">The AHN-3 surface dataset path.</param>
	/// <param name="outputPath">The output directory path.</param>
	InMemoryProcess(const std::string& id,
	                const std::string& ahn2SurfacePath, const std::string& ahn3SurfacePath,
	                const std::string& outputPath);
	
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="id">Unique identifier, in most cases the name of the tile to process.</param>
	/// <param name="ahn2SurfacePath">The AHN-2 surface dataset path.</param>
	/// <param name="ahn3SurfacePath">The AHN-3 surface dataset path.</param>
	/// <param name="ahn2TerrainPath">The AHN-2 terrain dataset path.</param>
	/// <param name="ahn3TerrainPath">The AHN-3 terrain dataset path.</param>
	/// <param name="outputPath">The output directory path.</param>
	InMemoryProcess(const std::string& id,
	                const std::string& ahn2SurfacePath, const std::string& ahn3SurfacePath,
	                const std::string& ahn2TerrainPath, const std::string& ahn3TerrainPath,
	                const std::string& outputPath);

protected:
	/// <summary>
	/// Produces the DEM and optionally a color-relief output.
	/// </summary>
	void onExecute() override;

	/// <summary>
	/// Creates a new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>New result on the heap.</returns>
	Result* createResult(const std::string& name, bool isFinal = false) override;

	/// <summary>
	/// Configures the output format options for the given transformation.
	/// </summary>
	/// <remarks>
	/// The targets of these transformations are never final.
	/// </remarks>
	void configure(CloudLib::DEM::Transformation& transformation) const override;
};

/// <summary>
/// The AHN Building Filter operation with intermediate results stored on the drive.
/// </summary>
/// <remarks>
/// Supports debugging by persisting the intermediate results.
/// </remarks>
class FileBasedProcess : public InMemoryProcess
{
public:
	/// <summary>
	/// Keep intermediate results on disk after progress.
	/// </summary>
	bool debug = false;

protected:
	std::size_t _nextResult = 1;

public:	
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="id">Unique identifier, in most cases the name of the tile to process.</param>
	/// <param name="ahn2SurfacePath">The AHN-2 surface dataset path.</param>
	/// <param name="ahn3SurfacePath">The AHN-3 surface dataset path.</param>
	/// <param name="outputPath">The output directory path.</param>
	FileBasedProcess(const std::string& id,
	                 const std::string& ahn2SurfacePath, const std::string& ahn3SurfacePath,
	                 const std::string& outputPath)
		: InMemoryProcess(id, ahn2SurfacePath, ahn3SurfacePath, outputPath)
	{ }
	
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="id">Unique identifier, in most cases the name of the tile to process.</param>
	/// <param name="ahn2SurfacePath">The AHN-2 surface dataset path.</param>
	/// <param name="ahn3SurfacePath">The AHN-3 surface dataset path.</param>
	/// <param name="ahn2TerrainPath">The AHN-2 terrain dataset path.</param>
	/// <param name="ahn3TerrainPath">The AHN-3 terrain dataset path.</param>
	/// <param name="outputPath">The output directory path.</param>
	FileBasedProcess(const std::string& id,
	                 const std::string& ahn2SurfacePath, const std::string& ahn3SurfacePath,
	                 const std::string& ahn2TerrainPath, const std::string& ahn3TerrainPath,
	                 const std::string& outputPath)
		: InMemoryProcess(id,
		                  ahn2SurfacePath, ahn3SurfacePath,
		                  ahn2TerrainPath, ahn3TerrainPath,
		                  outputPath)
	{ }

protected:
	/// <summary>
	/// Creates a new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>New result on the heap.</returns>
	Result* createResult(const std::string& name, bool isFinal = false) override;

	/// <summary>
	/// Configures the output format options for the given transformation.
	/// </summary>
	/// <remarks>
	/// The targets of these transformations are never final.
	/// </remarks>
	void configure(CloudLib::DEM::Transformation& transformation) const override;
};

/// <summary>
/// The AHN Building Filter operation with streamed input and output.
/// </summary>
/// <remarks>
/// The input stream may contain 2 or 4 bands in the following order:
/// AHN-2 surface DEM, AHN-3 surface DEM, AHN-2 terrain DEM, AHN-3 terrain DEM.
/// </remarks>
class StreamedProcess : public Process
{
protected:
	/// <summary>
	/// The stream input path.
	/// </summary>
	static const char* StreamInputPath;
	VSILFILE* _streamInputFile = nullptr;
	std::vector<GByte>* _buffer = nullptr;

	std::size_t _nextResult = 1;

public:	
	/// <summary>
	/// Initializes a new instance of the class. Read streamed input.
	/// </summary>
	/// <param name="id">Unique identifier, in most cases the name of the tile to process.</param>
	StreamedProcess(const std::string& id);
	~StreamedProcess();

protected:
	/// <summary>
	/// Produces the output and streams.
	/// </summary>
	void onExecute() override;

	/// <summary>
	/// Creates a new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>New result on the heap.</returns>
	Result* createResult(const std::string& name, bool isFinal = false) override;

	/// <summary>
	/// Configures the output format options for the given transformation.
	/// </summary>
	/// <remarks>
	/// The targets of these transformations are never final.
	/// </remarks>
	void configure(CloudLib::DEM::Transformation& transformation) const override;
};

/// <summary>
/// The AHN Building Filter operation with Hadoop Streaming support.
/// </summary>
class HadoopProcess : public StreamedProcess
{
public:	
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <remarks>
	/// The identifier which will be the filename will be read from the stream.
	/// </remarks>
	HadoopProcess() : StreamedProcess("placeholder")
	{ }

protected:
	/// <summary>
	/// Verifies the configuration, prepares streaming, handles Hadoop Streaming API specialities.
	/// </summary>
	void onPrepare() override;
};
} // Buildings
} // AHN
