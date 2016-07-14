#include <iosfwd>
#include <vector>
#include <functional>
#include <iterator>
#include <utility>
#include <stdexcept>

#ifdef _MSC_VER
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#endif

#include <gdal_utils.h>

#include <CloudTools.Common/Reporter.h>
#include <CloudLib.DEM/SweepLineTransformation.h>
#include "Process.h"
#include "BuildingExtraction.h"
#include "BuildingFilter.h"
#include "Comparison.h"
#include "NoiseFilter.h"
#include "MajorityFilter.h"
#include "ClusterFilter.h"
#include "MorphologyFilter.h"

using namespace CloudLib::DEM;
using namespace CloudTools::IO;

namespace AHN
{
namespace Buildings
{
#pragma region Process

Process::~Process()
{
	if (_ahn2SurfaceDataset) GDALClose(_ahn2SurfaceDataset);
	if (_ahn3SurfaceDataset) GDALClose(_ahn3SurfaceDataset);
	if (_ahn2TerrainDataset) GDALClose(_ahn2TerrainDataset);
	if (_ahn3TerrainDataset) GDALClose(_ahn3TerrainDataset);

	for (auto& item : _results)
		delete item.second;
	_results.clear();
}

void Process::onPrepare()
{
	if (_ahn2SurfaceDataset == nullptr ||
		_ahn3SurfaceDataset == nullptr)
		throw std::runtime_error("Defining the surface DEM files is mandatory.");

	// Piping the internal progress reporter to override message.
	_progress = [this](float complete, std::string message)
		{
			return this->progress(complete, this->_progressMessage);
		};
}

void Process::onExecute()
{
	// Building filtering / extraction
	newResult("buildings-2");
	newResult("buildings-3");
	if(_ahn2TerrainDataset && _ahn3TerrainDataset)
	{
		_progressMessage = "Building extraction / AHN-2";
		{
			BuildingExtraction extraction(_ahn2SurfaceDataset, _ahn2TerrainDataset,
				result("buildings-2").path(), _progress);
			configure(extraction);

			extraction.execute();
			result("buildings-2").dataset = extraction.target();
		}

		_progressMessage = "Building extraction / AHN-3";
		{
			BuildingExtraction extraction(_ahn3SurfaceDataset, _ahn3TerrainDataset,
				result("buildings-3").path(), _progress);
			configure(extraction);

			extraction.execute();
			result("buildings-3").dataset = extraction.target();
		}

		GDALClose(_ahn2TerrainDataset); _ahn2TerrainDataset = nullptr;
		GDALClose(_ahn3TerrainDataset); _ahn3TerrainDataset = nullptr;
	}
	else
	{
		_progressMessage = "Building filtering / AHN-2";
		{
			BuildingFilter filter(_ahn2SurfaceDataset,
				result("buildings-2").path(), _progress);
			configure(filter);

			filter.execute();
			result("buildings-2").dataset = filter.target();
		}

		_progressMessage = "Building filtering / AHN-3";
		{
			BuildingFilter filter(_ahn3SurfaceDataset,
				result("buildings-3").path(), _progress);
			configure(filter);

			filter.execute();
			result("buildings-3").dataset = filter.target();
		}
	}

	// Create basic changeset
	_progressMessage = "Creating changeset";
	newResult("changeset");
	{
		Comparison comparison(_ahn2SurfaceDataset, _ahn3SurfaceDataset, 
							  result("buildings-2").dataset, result("buildings-3").dataset,
		                      result("changeset").path(), _progress);
		comparison.minimumThreshold = 1.f;
		comparison.spatialReference = "EPSG:28992"; // The SRS is given slightly differently for some AHN-2 tiles (but not all).
		configure(comparison);

		comparison.execute();
		result("changeset").dataset = comparison.target();
	}
	GDALClose(_ahn2SurfaceDataset); _ahn2SurfaceDataset = nullptr;
	GDALClose(_ahn3SurfaceDataset); _ahn3SurfaceDataset = nullptr;
	deleteResult("buildings-2");
	deleteResult("buildings-3");

	// Noise filtering
	_progressMessage = "Noise filtering";
	newResult("noise");
	{
		NoiseFilter filter(result("changeset").dataset, result("noise").path(), 2, _progress);
		configure(filter);

		filter.execute();
		result("noise").dataset = filter.target();
	}
	deleteResult("changeset");

	// Cluster filtering
	_progressMessage = "Cluster filtering";
	newResult("sieve");
	newResult("cluster");
	{
		ClusterFilter filter(result("noise").dataset, result("sieve").path(), result("cluster").path(), _progress);
		filter.nodataValue = 0;
		configure(filter);

		filter.execute();
		result("sieve").dataset = filter.filter();
		result("cluster").dataset = filter.target();
	}
	deleteResult("noise");
	deleteResult("sieve");

	// Morpohology dilation
	_progressMessage = "Morpohology dilation";
	newResult("dilation");
	{
		MorphologyFilter filter(result("cluster").dataset, result("dilation").path(), MorphologyFilter::Dilation, _progress);
		configure(filter);

		filter.execute();
		result("dilation").dataset = filter.target();
	}
	deleteResult("cluster");

	// Majority filtering
	for (int range = 1; range <= 2; ++range)
	{
		_progressMessage = "Majority filtering / r=" + std::to_string(range);
		unsigned int index = newResult("majority");
		{
			MajorityFilter filter(
				index == 0 ? result("dilation").dataset : result("majority", 0).dataset,
				result("majority", index).path(),
				range, _progress);
			configure(filter);

			filter.execute();
			result("majority", index).dataset = filter.target();
		}
		if (index == 0)
			deleteResult("dilation");
		else
			deleteResult("majority", 0);
	}

	// Write out the results
	_progressMessage = "Writing results";
	newResult(std::string(), true);
	{
		// GTiff creation options
		char** params = nullptr;
		params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

		// Copy results to disk or VSI
		GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
		result("").dataset = driver->CreateCopy(result("").path().c_str(), result("majority").dataset,
		                                         false, params, gdalProgress, static_cast<void*>(this));
		CSLDestroy(params);
	}
	deleteResult("majority");
}

Result& Process::result(const std::string& name, unsigned int index)
{
	if (_results.count(name) <= index)
		throw std::out_of_range("No result found with the given name and index.");

	auto range = _results.equal_range(name);
	auto it = range.first;
	std::advance(it, index);
	return *it->second;
}

unsigned int Process::newResult(const std::string& name, bool isFinal)
{
	std::pair<std::string, Result*> item(name, createResult(name, isFinal));
	_results.emplace(std::move(item));
	return _results.count(name) - 1;
}

void Process::deleteResult(const std::string& name, unsigned int index)
{
	if (_results.count(name) <= index)
		throw std::out_of_range("No result found with the given name and index.");

	auto range = _results.equal_range(name);
	auto it = range.first;
	std::advance(it, index);
	delete it->second;
	_results.erase(it);
}

int Process::gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg)
{
	Process* process = static_cast<Process*>(pProgressArg);
	return process->progress(static_cast<float>(dfComplete), process->_progressMessage);
}

#pragma endregion

#pragma region InMemoryProcess

InMemoryProcess::InMemoryProcess(const std::string& id,
	const std::string& ahn2SurfacePath, const std::string& ahn3SurfacePath,
	const std::string& outputPath)
	: Process(id),
	_outputPath(outputPath)
{
	_ahn2SurfaceDataset = static_cast<GDALDataset*>(GDALOpen(ahn2SurfacePath.c_str(), GA_ReadOnly));
	_ahn3SurfaceDataset = static_cast<GDALDataset*>(GDALOpen(ahn3SurfacePath.c_str(), GA_ReadOnly));
	_ahn2TerrainDataset = nullptr;
	_ahn3TerrainDataset = nullptr;

	if (_ahn2SurfaceDataset == nullptr ||
		_ahn3SurfaceDataset == nullptr)
		throw std::runtime_error("Error at opening a source file.");
}

InMemoryProcess::InMemoryProcess(const std::string& id,
	const std::string& ahn2SurfacePath, const std::string& ahn3SurfacePath,
	const std::string& ahn2TerrainPath, const std::string& ahn3TerrainPath,
	const std::string& outputPath)
	: Process(id),
	_outputPath(outputPath)
{
	_ahn2SurfaceDataset = static_cast<GDALDataset*>(GDALOpen(ahn2SurfacePath.c_str(), GA_ReadOnly));
	_ahn3SurfaceDataset = static_cast<GDALDataset*>(GDALOpen(ahn3SurfacePath.c_str(), GA_ReadOnly));
	_ahn2TerrainDataset = static_cast<GDALDataset*>(GDALOpen(ahn2TerrainPath.c_str(), GA_ReadOnly));
	_ahn3TerrainDataset = static_cast<GDALDataset*>(GDALOpen(ahn3TerrainPath.c_str(), GA_ReadOnly));

	if (_ahn2SurfaceDataset == nullptr ||
		_ahn3SurfaceDataset == nullptr ||
		_ahn2TerrainDataset == nullptr ||
		_ahn3TerrainDataset == nullptr)
		throw std::runtime_error("Error at opening a source file.");
}

void InMemoryProcess::onExecute()
{
	Process::onExecute();
	if (this->colorFile.empty()) return;

	// Color relief
	_progressMessage = "Color relief";
	newResult("rgb", true);
	{
		char **params = nullptr;
		params = CSLAddString(params, "-alpha");
		params = CSLAddString(params, "-co");
		params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

		GDALDEMProcessingOptions *options = GDALDEMProcessingOptionsNew(params, nullptr);
		GDALDEMProcessingOptionsSetProgress(options, gdalProgress, static_cast<void*>(this));

		result("rgb").dataset = static_cast<GDALDataset*>(
			GDALDEMProcessing(
				result("rgb").path().c_str(), result("").dataset,
				"color-relief", colorFile.c_str(),
				options, nullptr)
			);
		GDALDEMProcessingOptionsFree(options);
		CSLDestroy(params);
	}
	deleteResult("rgb");
}

Result* InMemoryProcess::createResult(const std::string& name, bool isFinal)
{
	if (isFinal)
	{
		std::string filename = _id;
		if (name.length() > 0)
			filename += "_" + name;
		filename += ".tif";
		return new PermanentFileResult(fs::path(_outputPath) / filename);
	}
	else
		return new MemoryResult();
}

void InMemoryProcess::configure(CloudLib::DEM::Transformation& transformation) const
{
	transformation.targetFormat = "MEM";
}

#pragma endregion

#pragma region FileBasedProcess

Result* FileBasedProcess::createResult(const std::string& name, bool isFinal)
{
	std::string filename = _id;
	if (!isFinal)
		filename += "_" + std::to_string(_nextResult++);
	if (name.length() > 0)
		filename += "_" + name;
	filename += ".tif";

	if (isFinal || debug)
		return new PermanentFileResult(fs::path(_outputPath) / filename);
	else
		return new TemporaryFileResult(fs::path(_outputPath) / filename);
}

void FileBasedProcess::configure(CloudLib::DEM::Transformation& transformation) const
{
	transformation.targetFormat = "GTiff";
	transformation.createOptions.insert(std::make_pair("COMPRESS", "DEFLATE"));
}

#pragma endregion

#pragma region StreamedProcess

const char* StreamedProcess::StreamInputPath = "/vsimem/stream.tif";

StreamedProcess::StreamedProcess(const std::string& id)
	: Process(id)
{
	// Read streamed input into a virtual file.
	std::vector<GByte> buffer((
		std::istreambuf_iterator<char>(std::cin)),
		(std::istreambuf_iterator<char>()));
	_streamInputFile = VSIFileFromMemBuffer(StreamInputPath, &buffer[0], buffer.size(), false);

	_ahn2SurfaceDataset = static_cast<GDALDataset*>(GDALOpen(StreamInputPath, GA_ReadOnly));
	if (_ahn2SurfaceDataset->GetRasterCount() < 2)
		throw std::runtime_error("Streamed data must contain at least 2 (surface DEM) bands.");

	_ahn3SurfaceDataset = _ahn2SurfaceDataset;
	if (_ahn2SurfaceDataset->GetRasterCount() < 4)
	{
		_ahn2TerrainDataset = nullptr;
		_ahn3TerrainDataset = nullptr;
	}
	else
	{
		_ahn2TerrainDataset = _ahn2SurfaceDataset;
		_ahn3TerrainDataset = _ahn2SurfaceDataset;
	}
}

StreamedProcess::~StreamedProcess()
{
	// Closes streamed input
	if (_streamInputFile)
	{
		VSIFCloseL(_streamInputFile);
		VSIUnlink(StreamInputPath);
	}
}

void StreamedProcess::onPrepare()
{
	Process::onPrepare();

	// Prepares streaming
	#ifdef _MSC_VER
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	#endif
}

void StreamedProcess::onExecute()
{
	Process::onExecute();

	// Closes streamed input
	if (_streamInputFile)
	{
		VSIFCloseL(_streamInputFile);
		VSIUnlink(StreamInputPath);
		_streamInputFile = nullptr;
	}

	// Stream the output
	vsi_l_offset length;
	GByte* buffer = VSIGetMemFileBuffer(result("").path().c_str(), &length, true);
	std::copy(buffer, buffer + length, std::ostream_iterator<GByte>(std::cout));
	delete[] buffer;
}

Result* StreamedProcess::createResult(const std::string& name, bool isFinal)
{
	if (isFinal)
	{
		std::string filename = _id;
		if (name.length() > 0)
			filename += "_" + name;
		filename += ".tif";
		return new VirtualResult(filename);
	}
	else
		return new MemoryResult();
}

void StreamedProcess::configure(CloudLib::DEM::Transformation& transformation) const
{
	transformation.targetFormat = "MEM";
}

#pragma endregion

#pragma region HadoopProcess

void HadoopProcess::onPrepare()
{
	std::string key;
	std::cin >> key;
	std::cin.get(); // tabulator
	std::cout << key << '\t';
	_id = fs::path(key).stem().string();

	StreamedProcess::onPrepare();
}

#pragma endregion
} // Buildings
} // AHN
