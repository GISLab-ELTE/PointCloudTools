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

#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/SweepLineTransformation.hpp>
#include <CloudTools.DEM/Filters/NoiseFilter.hpp>
#include <CloudTools.DEM/Filters/MajorityFilter.hpp>
#include <CloudTools.DEM/Filters/ClusterFilter.hpp>
#include <CloudTools.DEM/Filters/MorphologyFilter.hpp>
#include "Process.h"
#include "BuildingChangeDetection.hpp"
#include "BuildingExtraction.h"
#include "ContourDetection.h"
#include "ContourFiltering.h"
#include "ContourSplitting.h"
#include "ContourSimplification.h"
#include "ContourClassification.h"
#include "ContourConvexHullRasterizer.h"
#include "Comparison.h"

using namespace CloudTools::DEM;
using namespace CloudTools::IO;

namespace AHN
{
namespace Buildings
{
#pragma region Process

Process::~Process()
{
	if (_ahn2SurfaceDataset)
		GDALClose(_ahn2SurfaceDataset);
	if (_ahn3SurfaceDataset && _ahn3SurfaceDataset != _ahn2SurfaceDataset)
		GDALClose(_ahn3SurfaceDataset);
	if (_ahn2TerrainDataset)
		GDALClose(_ahn2TerrainDataset);
	if (_ahn3TerrainDataset && _ahn3TerrainDataset != _ahn2TerrainDataset)
		GDALClose(_ahn3TerrainDataset);
}

void Process::onPrepare()
{
	if (_ahn2SurfaceDataset == nullptr ||
		_ahn3SurfaceDataset == nullptr)
		throw std::runtime_error("Defining the surface DEM files is mandatory.");

	// Piping the internal progress reporter to override message.
	if (progress)
		_progress = [this](float complete, std::string message)
	{
		return this->progress(complete, this->_progressMessage);
	};
	else
		_progress = nullptr;
}

void Process::onExecute()
{
	// Building filtering / extraction
	newResult("buildings-ahn2");
	newResult("buildings-ahn3");
	if (_ahn2TerrainDataset && _ahn3TerrainDataset)
	{
		_progressMessage = "Building extraction / AHN-2";
		{
			BuildingExtraction extraction(_ahn2SurfaceDataset, _ahn2TerrainDataset,
				result("buildings-ahn2").path(), _progress);
			if (_ahn2SurfaceDataset == _ahn2TerrainDataset)
				extraction.bands = { 1, 2 };
			configure(extraction);

			extraction.execute();
			result("buildings-ahn2").dataset = extraction.target();
		}

		_progressMessage = "Building extraction / AHN-3";
		{
			BuildingExtraction extraction(_ahn3SurfaceDataset, _ahn3TerrainDataset,
				result("buildings-ahn3").path(), _progress);
			if (_ahn3SurfaceDataset == _ahn3TerrainDataset)
				extraction.bands = { 1, 2 };
			if (_ahn2SurfaceDataset == _ahn3SurfaceDataset)
				extraction.bands = { 3, 4 };
			configure(extraction);

			extraction.execute();
			result("buildings-ahn3").dataset = extraction.target();
		}
	}
	else
	{
		_progressMessage = "Building segmentation / AHN-2";
		{
			ContourDetection cd(_ahn2SurfaceDataset, _progress);
			cd.execute();

			ContourFiltering cf(cd.getContours());
			cf.execute();

			ContourSplitting cs(cf.getContours());
			cs.execute();

			ContourSimplification cs2(cs.getContours());
			cs2.execute();

			ContourClassification cc(cs2.getContours());
			cc.execute();

			ContourConvexHullRasterizer cr(_ahn2SurfaceDataset, cc.getContours(), result("buildings-ahn2").path(), _progress);
			configure(cr);
			cr.execute();
			result("buildings-ahn2").dataset = cr.target();
		}

		_progressMessage = "Building segmentation / AHN-3";
		{
			ContourDetection cd(_ahn3SurfaceDataset, _progress);
			cd.execute();

			ContourFiltering cf(cd.getContours());
			cf.execute();

			ContourSplitting cs(cf.getContours());
			cs.execute();

			ContourSimplification cs2(cs.getContours());
			cs2.execute();

			ContourClassification cc(cs2.getContours());
			cc.execute();

			ContourConvexHullRasterizer cr(_ahn3SurfaceDataset, cc.getContours(), result("buildings-ahn3").path(), _progress);
			configure(cr);
			cr.execute();
			result("buildings-ahn3").dataset = cr.target();
		}

		_progressMessage = "Segmentation based change detection";
		newResult("changes");
		{
			BuildingChangeDetection comp(result("buildings-ahn2").dataset, result("buildings-ahn3").dataset,
				                         _ahn2SurfaceDataset, _ahn3SurfaceDataset,
				                         result("changes").path(), _progress);
			configure(comp);
			comp.execute();
			result("changes").dataset = comp.target();
		}

		_progressMessage = "Writing results";
		newResult("segmented", true);
		{
			// GTiff creation options
			char** params = nullptr;
			params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

			// Copy results to disk or VSI
			GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
			result("segmented").dataset = driver->CreateCopy(result("segmented").path().c_str(), result("changes").dataset,
				                                             false, params, gdalProgress, static_cast<void*>(this));
			CSLDestroy(params);
		}
		deleteResult("changes");
	}

	// Create basic changeset
	_progressMessage = "Creating changeset";
	newResult("changeset");
	{
		Comparison comparison(_ahn2SurfaceDataset, _ahn3SurfaceDataset,
			                  result("buildings-ahn2").dataset, result("buildings-ahn3").dataset,
			                  result("changeset").path(), _progress);
		comparison.minimumThreshold = 1.f;
		comparison.spatialReference = "EPSG:28992"; // The SRS is given slightly differently for some AHN-2 tiles (but not all).
		if (_ahn2TerrainDataset && _ahn3TerrainDataset &&
			_ahn2SurfaceDataset == _ahn3SurfaceDataset)
			comparison.bands = { 1, 3 };
		configure(comparison);

		comparison.execute();
		result("changeset").dataset = comparison.target();
	}
	GDALClose(_ahn2SurfaceDataset);
	if (_ahn3SurfaceDataset != _ahn2SurfaceDataset)
		GDALClose(_ahn3SurfaceDataset);
	if (_ahn2TerrainDataset != _ahn2SurfaceDataset)
		GDALClose(_ahn2TerrainDataset);
	if (_ahn3TerrainDataset != _ahn3SurfaceDataset)
		GDALClose(_ahn3TerrainDataset);
	_ahn2SurfaceDataset = nullptr;
	_ahn3SurfaceDataset = nullptr;
	_ahn2TerrainDataset = nullptr;
	_ahn3TerrainDataset = nullptr;
	deleteResult("buildings-ahn2");
	deleteResult("buildings-ahn3");

	// Noise filtering
	_progressMessage = "Noise filtering";
	newResult("noise");
	{
		NoiseFilter<float> filter(result("changeset").dataset, result("noise").path(), 2, _progress);
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
		ClusterFilter<float> filter(result("noise").dataset, result("sieve").path(), result("cluster").path(), _progress);
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
		MorphologyFilter<float> filter(result("cluster").dataset, result("dilation").path(), MorphologyFilter<float>::Dilation, _progress);
		configure(filter);

		filter.execute();
		result("dilation").dataset = filter.target();
	}
	deleteResult("cluster");

	// Majority filtering
	for (int range = 1; range <= 2; ++range)
	{
		_progressMessage = "Majority filtering / r=" + std::to_string(range);
		std::size_t index = newResult("majority");
		{
			MajorityFilter<float> filter(
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

int Process::gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg)
{
	Process* process = static_cast<Process*>(pProgressArg);
	if (!process->progress) return true;
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

void InMemoryProcess::configure(CloudTools::DEM::Transformation& transformation) const
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

void FileBasedProcess::configure(CloudTools::DEM::Transformation& transformation) const
{
	transformation.targetFormat = "GTiff";
	transformation.createOptions.insert(std::make_pair("COMPRESS", "DEFLATE"));
}

#pragma endregion

#pragma region StreamedProcess

const char* StreamedProcess::StreamInputPath = "/vsimem/stream.tif";

StreamedProcess::~StreamedProcess()
{
	// Closes streamed input
	if (_streamInputFile)
	{
		VSIFCloseL(_streamInputFile);
		VSIUnlink(StreamInputPath);
	}
	if (_buffer)
	{
		delete _buffer;
	}
}

void StreamedProcess::onPrepare()
{
	#ifdef _MSC_VER
	// Prepares input binary read mode on Windows
	_setmode(_fileno(stdin), _O_BINARY);
	#endif

	// Read streamed input into a virtual file.
	_buffer = new std::vector<GByte>(
		(std::istreambuf_iterator<char>(std::cin)),
		(std::istreambuf_iterator<char>()));
	_streamInputFile = VSIFileFromMemBuffer(StreamInputPath, &(*_buffer)[0], _buffer->size(), false);

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
	if (_buffer)
	{
		delete _buffer;
		_buffer = nullptr;
	}

	#ifdef _MSC_VER
	// Prepares output binary write mode on Windows
	_setmode(_fileno(stdout), _O_BINARY);
	#endif

	// Close the dataset to finalize the binary data behind it.
	GDALClose(result("").dataset);
	result("").dataset = nullptr;

	// Stream the output
	vsi_l_offset length;
	GByte* buffer = VSIGetMemFileBuffer(result("").path().c_str(), &length, false);
	std::copy(buffer, buffer + length, std::ostream_iterator<GByte>(std::cout));
	// Do not delete buffer, as it is still owned and freed by VSI file.
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

void StreamedProcess::configure(CloudTools::DEM::Transformation& transformation) const
{
	transformation.targetFormat = "MEM";
}

#pragma endregion

#pragma region HadoopProcess

void HadoopProcess::onPrepare()
{
	std::cin >> _key;
	std::cin.get(); // tabulator
	std::cout << _key << '\t';
	_id = fs::path(_key).stem().string();

	StreamedProcess::onPrepare();
}

#pragma endregion
} // Buildings
} // AHN
