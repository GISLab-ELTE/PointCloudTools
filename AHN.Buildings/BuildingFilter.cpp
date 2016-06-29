#include <iosfwd>
#include <vector>
#include <algorithm>
#include <functional>
#include <iterator>
#include <cmath>

#ifdef _MSC_VER
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#endif

#include <gdal_alg.h>
#include <gdal_utils.h>

#include <CloudTools.Common/Reporter.h>
#include <CloudLib.DEM/SweepLineTransformation.h>
#include "BuildingFilter.h"

using namespace CloudLib::DEM;
using namespace CloudTools::IO;

namespace AHN
{
const std::string BuildingFilter::StreamFile = "/vsimem/stream.tif";
const float BuildingFilter::Threshold = 1.0f;

void BuildingFilter::onPrepare()
{
	if (hasFlag(_mode, IOMode::Stream))
	{
		if (hasFlag(_mode, IOMode::Hadoop))
		{
			std::string key;
			std::cin >> key;
			std::cin.get(); // tabulator
			std::cout << key << '\t';

			tileName = fs::path(key).stem().string();
		}

		#ifdef _MSC_VER
		_setmode(_fileno(stdin), _O_BINARY);
		_setmode(_fileno(stdout), _O_BINARY);
		#endif
	}

	_tileGroup = tileName.substr(0, 3);
	_resultFormat = hasFlag(_mode, IOMode::Files) ? "GTiff" : "MEM";

	if (!colorFile.empty() && !fs::is_regular_file(colorFile))
		throw std::runtime_error("Color file does not exists.");

	_progress = [this](float complete, std::string message)
	{
		return this->progress(complete, this->_progressMessage);
	};
}

void BuildingFilter::onExecute()
{
	// Create basic changeset
	_progressMessage = "Creating changeset";
	createResult();
	if (!hasFlag(_mode, IOMode::Stream))
	{
		// Collect input files
		std::vector<fs::path> ahn2Files;
		for (fs::directory_iterator itGroup(_ahn2Dir); itGroup != fs::directory_iterator(); ++itGroup)
		{
			if (fs::is_directory(itGroup->path()) &&
				itGroup->path().filename().string().find(_tileGroup) != std::string::npos)
			{
				for (fs::directory_iterator itTile(itGroup->path()); itTile != fs::directory_iterator(); ++itTile)
				{
					if (fs::is_directory(itTile->path()) &&
						itTile->path().filename().string().find(tileName) != std::string::npos)
						ahn2Files.push_back(itTile->path());
				}
			}
		}

		fs::path ahn3File;
		for (fs::directory_iterator itTile(_ahn3Dir); itTile != fs::directory_iterator(); ++itTile)
		{
			if (fs::is_regular_file(itTile->path()) &&
				itTile->path().filename().string().find(tileName) != std::string::npos)
			{
				ahn3File = itTile->path();
				break;
			}
		}

		std::vector<std::string> ahnFiles(ahn2Files.size() + 1);
		ahnFiles[0] = ahn3File.string();
		std::transform(ahn2Files.begin(), ahn2Files.end(), ahnFiles.begin() + 1,
			[](fs::path ahn2File)
		{
			return ahn2File.string();
		});

		// Creating changeset
		SweepLineTransformation<float> comparison(ahnFiles, _resultPaths[0].string(), nullptr, _progress);
		comparison.targetFormat = _resultFormat;
		comparison.computation = [&comparison]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			auto ahn3 = sources.begin();
			auto ahn2 = std::find_if(ahn3 + 1, sources.end(),
				[](const Window<float>& source)
			{
				return source.hasData();
			});

			if (ahn2 == sources.end() || !ahn3->hasData())
				return static_cast<float>(comparison.nodataValue);

			float difference = ahn3->data() - ahn2->data();
			if (std::abs(difference) >= 1000 || std::abs(difference) <= Threshold)
				difference = static_cast<float>(comparison.nodataValue);
			return difference;
		};
		comparison.nodataValue = 0;

		comparison.execute();
		_resultDatasets[0] = comparison.target();
	}
	else
	{
		std::vector<GByte> buffer((
			std::istreambuf_iterator<char>(std::cin)),
			(std::istreambuf_iterator<char>()));
		VSILFILE* vsiFile = VSIFileFromMemBuffer(StreamFile.c_str(), &buffer[0], buffer.size(), false);

		// Creating changeset
		SweepLineTransformation<float> comparison({ StreamFile, StreamFile }, _resultPaths[0].string(), nullptr, _progress);
		comparison.targetFormat = _resultFormat;
		comparison.computation = [&comparison]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& ahn2 = sources[0];
			const Window<float>& ahn3 = sources[1];

			float difference = ahn3.data() - ahn2.data();
			if (std::abs(difference) >= 1000 || std::abs(difference) <= Threshold)
				difference = static_cast<float>(comparison.nodataValue);
			return difference;
		};
		comparison.nodataValue = 0;

		comparison.execute();
		_resultDatasets[0] = comparison.target();
		VSIFCloseL(vsiFile);
		VSIUnlink(StreamFile.c_str());
	}

	// Noise filtering
	_progressMessage = "Noise filtering";
	createResult();
	{
		int range = 2;
		SweepLineTransformation<float> noiseFilter({ _resultDatasets[0] }, _resultPaths[1].string(), range, nullptr, _progress);
		noiseFilter.targetFormat = _resultFormat;
		noiseFilter.computation = [&noiseFilter, range]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];
			if (!source.hasData()) return static_cast<float>(noiseFilter.nodataValue);

			float noise = 0;
			int counter = -1;
			for (int i = -range; i <= range; ++i)
				for (int j = -range; j <= range; ++j)
					if (source.hasData(i, j))
					{
						noise += std::abs(source.data() - source.data(i, j));
						++counter;
					}

			if (counter == 0) return static_cast<float>(noiseFilter.nodataValue);
			if (noise / counter > 1) return static_cast<float>(noiseFilter.nodataValue);
			else return source.data();

		};
		noiseFilter.nodataValue = 0;

		noiseFilter.execute();
		_resultDatasets[1] = noiseFilter.target();
	}
	deleteResult(); // basic changeset

	// Binarization
	_progressMessage = "Sieve filtering / prepare";
	createResult();
	{
		SweepLineTransformation<GByte, float> binarization({ _resultDatasets[0] }, _resultPaths[1].string(), 0, nullptr, _progress);
		binarization.targetFormat = _resultFormat;
		binarization.computation = [&binarization]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];
			if (!source.hasData()) return 1;
			else return 255;

		};
		binarization.nodataValue = 0;

		binarization.execute();
		_resultDatasets[1] = binarization.target();
	}

	// Sieve filtering
	_progressMessage = "Sieve filtering / execute";
	{
		GDALRasterBand* band = _resultDatasets[1]->GetRasterBand(1);

		GDALSieveFilter(
			band, nullptr, band,
			400, 4,
			nullptr, gdalProgress, static_cast<void*>(this));
	}

	// Cluster filtering
	_progressMessage = "Cluster filtering";
	createResult();
	{
		std::vector<GDALDataset*> sources = { _resultDatasets[0], _resultDatasets[1] };
		SweepLineTransformation<float> clusterFilter(sources, _resultPaths[2].string(), 0, nullptr, _progress);
		clusterFilter.targetFormat = _resultFormat;
		clusterFilter.computation = [&clusterFilter]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& noise = sources[0];
			const Window<float>& sieve = sources[1];

			if (sieve.hasData() && sieve.data() == 255) return noise.data();
			else return static_cast<float>(clusterFilter.nodataValue);

		};
		clusterFilter.nodataValue = 0;

		clusterFilter.execute();
		_resultDatasets[2] = clusterFilter.target();
	}
	deleteResult(); // noise filter
	deleteResult(); // sieve filter

	// Majority filtering
	for (int range = 1; range <= 2; ++range)
	{
		_progressMessage = "Majority filtering / r=" + std::to_string(range);
		createResult();
		{
			SweepLineTransformation<float> majorityFilter({ _resultDatasets[0] }, _resultPaths[1].string(), range, nullptr, _progress);
			majorityFilter.computation = [&majorityFilter, range]
			(int x, int y, const std::vector<Window<float>>& sources)
			{
				const Window<float>& source = sources[0];

				float sum = 0;
				int counter = 0;
				for (int i = -range; i <= range; ++i)
					for (int j = -range; j <= range; ++j)
						if (source.hasData(i, j))
						{
							sum += source.data(i, j);
							++counter;
						}
				if (counter < ((range + 2) * (range + 2) / 2)) return static_cast<float>(majorityFilter.nodataValue);
				else return source.hasData() ? source.data() : sum / counter;

			};
			majorityFilter.targetFormat = _resultFormat;
			majorityFilter.nodataValue = 0;

			majorityFilter.execute();
			_resultDatasets[1] = majorityFilter.target();
		}
		deleteResult(); // cluster filter or
						// previous majority filter
	}

	// Write out the results
	_progressMessage = "Writing results";
	createResult(hasFlag(_mode, IOMode::Stream) ? StreamFile : std::string());
	{
		// GTiff creation options
		char **params = nullptr;
		params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

		// Copy results to disk or VSI
		GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
		_resultDatasets[1] = driver->CreateCopy(_resultPaths[1].string().c_str(), _resultDatasets[0],
		                                        false, params, gdalProgress, static_cast<void*>(this));
		CSLDestroy(params);

		// Stream the output
		if (hasFlag(_mode, IOMode::Stream))
		{
			vsi_l_offset length;
			GByte* buffer = VSIGetMemFileBuffer(StreamFile.c_str(), &length, true);
			std::copy(buffer, buffer + length, std::ostream_iterator<GByte>(std::cout));
			delete[] buffer;
			VSIUnlink(StreamFile.c_str());
		}
	}

	// No color relief
	if (colorFile.empty() || !fs::is_regular_file(colorFile) || hasFlag(_mode, IOMode::Stream))
	{
		deleteResult();     // last majority filter
		deleteResult(true); // building filer result
		return;
	}

	// Color relief
	_progressMessage = "Color relief";
	createResult("rgb");
	{
		char **params = nullptr;
		params = CSLAddString(params, "-alpha");
		params = CSLAddString(params, "-co");
		params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

		GDALDEMProcessingOptions *options = GDALDEMProcessingOptionsNew(params, nullptr);
		GDALDEMProcessingOptionsSetProgress(options, gdalProgress, static_cast<void*>(this));

		_resultDatasets[2] = static_cast<GDALDataset*>(
			GDALDEMProcessing(
				_resultPaths[2].string().c_str(), _resultDatasets[0],
				"color-relief", colorFile.string().c_str(),
				options, nullptr)
			);
		GDALDEMProcessingOptionsFree(options);
		CSLDestroy(params);
	}

	deleteResult();     // last majority filter
	deleteResult(true); // building filer result
	deleteResult(true); // color relief
}

unsigned int BuildingFilter::createResult()
{
	_resultDatasets.push_back(nullptr);
	_resultPaths.push_back(hasFlag(_mode, IOMode::Files)
		? _outputDir / (tileName + "_" + std::to_string(_nextResult++) + ".tif")
		: "");
	return _resultDatasets.size() - 1;
}

unsigned int BuildingFilter::createResult(std::string suffix)
{
	std::string filename = suffix;
	if (suffix.length() > 0)
		filename = "_" + filename;
	filename = tileName + filename + ".tif";

	_resultDatasets.push_back(nullptr);
	_resultPaths.push_back(_outputDir / filename);
	return _resultDatasets.size() - 1;
}

void BuildingFilter::deleteResult(bool keepFile)
{
	if (_resultDatasets.size() == 0)
		throw std::underflow_error("No intermediate result to pop.");

	if (_resultDatasets.front() != nullptr)
		GDALClose(_resultDatasets.front());
	if (hasFlag(_mode, IOMode::Files) && !keepFile && !debug)
		fs::remove(_resultPaths.front());

	_resultDatasets.pop_front();
	_resultPaths.pop_front();
}

int BuildingFilter::gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg)
{
	BuildingFilter *filter = static_cast<BuildingFilter*>(pProgressArg);
	return filter->progress(static_cast<float>(dfComplete), filter->_progressMessage);
}
} // AHN