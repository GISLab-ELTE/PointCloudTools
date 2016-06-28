#pragma once

#include <string>
#include <deque>

#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include <CloudLib.DEM/Operation.h>
#include "IOMode.h"

namespace fs = boost::filesystem;

namespace AHN
{
/// <summary>
/// The AHN Building filter operation.
/// </summary>
class BuildingFilter : public CloudLib::DEM::Operation
{
public:
	/// <summary>
	/// The name of the tile to process.
	/// </summary>
	std::string tileName;

	/// <summary>
	/// Map file for color relief.
	/// </summary>
	/// <seealso href="http://www.gdal.org/gdaldem.html"/>
	fs::path colorFile;

	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

	/// <summary>
	/// Keep intermediate results on disk after progress.
	/// </summary>
	/// <remarks>
	/// Debug mode has no effect with in-memory intermediate results.
	/// </remarks>
	bool debug = false;

protected:
	IOMode _mode;
	fs::path _ahn2Dir, _ahn3Dir, _outputDir;
	std::string _tileGroup;

	ProgressType _progress;
	std::string _progressMessage;

private:
	static const std::string StreamFile;
	static const float Threshold;

	std::string _resultFormat;
	std::deque<fs::path> _resultPaths;
	std::deque<GDALDataset*> _resultDatasets;
	int _nextResult = 1;

public:	
	/// <summary>
	/// Initializes a new instance of the class with file-based processing.
	/// </summary>
	/// <param name="tileName">Name of the tile.</param>
	/// <param name="ahn2Dir">AHN-2 directory path.</param>
	/// <param name="ahn3Dir">AHN-3 directory path.</param>
	/// <param name="outputDir">Result directory path.</param>
	/// <param name="inMemory">Keep intermediate results in the memory.</param>
	BuildingFilter(std::string tileName,
	               fs::path ahn2Dir, fs::path ahn3Dir, fs::path outputDir,
	               bool inMemory = true)
		: tileName(tileName), _ahn2Dir(ahn2Dir), _ahn3Dir(ahn3Dir), _outputDir(outputDir),
		  _mode(inMemory ? IOMode::Memory : IOMode::Files)
	{ }

	/// <summary>
	/// Initializes a new instance of the class with streaming processing.
	/// </summary>
	/// <param name="tileName">Name of the tile.</param>
	BuildingFilter(std::string tileName)
		: tileName(tileName),
		  _mode(IOMode::Stream)
	{ }

	/// <summary>
	/// Initializes a new instance of the class with Hadoop processing.
	/// </summary>
	BuildingFilter()
		: _mode(IOMode::Hadoop)
	{ }

protected:
	/// <summary>
	/// Verifies the configuration, prepares streaming if necessary.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Produces the output file(s).
	/// </summary>
	void onExecute() override;

private:	
	/// <summary>
	/// Creates a new intermediate result.
	/// </summary>
	/// <returns>Index of the result.</returns>
	unsigned int createResult();	

	/// <summary>
	/// Creates a new intermediate result on the disk.
	/// </summary>
	/// <param name="suffix">The filename suffix following the tilename.</param>
	/// <returns>Index of the result.</returns>
	unsigned int createResult(std::string suffix);
	
	/// <summary>
	/// Deletes the oldest intermediate result.
	/// </summary>
	/// <param name="keepFile">Keep the result file if it resides on the filesystem.</param>
	void deleteResult(bool keepFile = false);

	/// <summary>
	/// Routes the C-style GDAL progress reports to the defined reporter.
	/// </summary>
	static int CPL_STDCALL gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg);
};
} // AHN