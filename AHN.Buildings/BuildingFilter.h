#pragma once

#include <string>
#include <deque>

#include <boost/filesystem.hpp>

#include <CloudLib.DEM/Operation.h>
#include <CloudLib.DEM/Transformation.h>
#include "IOMode.h"
#include "ResultFile.h"

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
	/// <summary>
	/// Virtual file for storing streamed input.
	/// </summary>
	static const std::string StreamInputFile;
	/// <summary>
	/// CompareThreshold for AHN 2-3 comparison.
	/// </summary>
	static const float CompareThreshold;

	std::deque<ResultFile> _results;
	int _nextResult = 1;

public:	
	/// <summary>
	/// Initializes a new instance of the class with file-based processing.
	/// </summary>
	/// <param name="tileName">Name of the tile.</param>
	/// <param name="ahn2Dir">AHN-2 directory path.</param>
	/// <param name="ahn3Dir">AHN-3 directory path.</param>
	/// <param name="outputDir">Result directory path.</param>
	static BuildingFilter* createPhysical(std::string tileName, fs::path ahn2Dir, fs::path ahn3Dir, fs::path outputDir)
	{
		BuildingFilter* filter = new BuildingFilter;
		filter->tileName = tileName;
		filter->_ahn2Dir = ahn2Dir;
		filter->_ahn3Dir = ahn3Dir;
		filter->_outputDir = outputDir;
		filter->_mode = IOMode::Files;
		return filter;
	}

	/// <summary>
	/// Initializes a new instance of the class with file-based and in-memory processing.
	/// </summary>
	/// <param name="tileName">Name of the tile.</param>
	/// <param name="ahn2Dir">AHN-2 directory path.</param>
	/// <param name="ahn3Dir">AHN-3 directory path.</param>
	/// <param name="outputDir">Result directory path.</param>
	static BuildingFilter* createInMemory(std::string tileName, fs::path ahn2Dir, fs::path ahn3Dir, fs::path outputDir)
	{
		BuildingFilter* filter = new BuildingFilter;
		filter->tileName = tileName;
		filter->_ahn2Dir = ahn2Dir;
		filter->_ahn3Dir = ahn3Dir;
		filter->_outputDir = outputDir;
		filter->_mode = IOMode::Memory;
		return filter;
	}

	/// <summary>
	/// Initializes a new instance of the class with streaming processing.
	/// </summary>
	/// <param name="tileName">Name of the tile.</param>
	static BuildingFilter* createStreamed(std::string tileName)
	{
		BuildingFilter* filter = new BuildingFilter;
		filter->tileName = tileName;
		filter->_mode = IOMode::Stream;
		return filter;
	}

	/// <summary>
	/// Initializes a new instance of the class with Hadoop processing.
	/// </summary>
	static BuildingFilter* createHadoop()
	{
		BuildingFilter* filter = new BuildingFilter;
		filter->_mode = IOMode::Hadoop;
		return filter;
	}

protected:
	BuildingFilter() { }

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
	/// Creates a new physical or in-memory result file.
	/// </summary>
	/// <param name="suffix">The filename suffix following the tilename.</param>
	/// <param name="isFinal">Do not apply numbering on the filename.</param>
	/// <returns>Index of the result.</returns>
	unsigned int createResult(std::string suffix, bool isFinal = false);
	
	/// <summary>
	/// Deletes the oldest intermediate result.
	/// </summary>
	/// <param name="isFinal">Keep the result file if it resides on the filesystem.</param>
	void deleteResult();

	/// <summary>
	/// Configures the output format options for the given transformation.
	/// </summary>
	void setIntermediateFormat(CloudLib::DEM::Transformation& transformation) const;

	/// <summary>
	/// Routes the C-style GDAL progress reports to the defined reporter.
	/// </summary>
	static int CPL_STDCALL gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg);
};
} // AHN