#include <iosfwd>
#include <vector>
#include <algorithm>
#include <functional>
#include <iterator>
#include <cmath>
#include <utility>

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
const std::string BuildingFilter::StreamInputFile = "/vsimem/stream.tif";
const float BuildingFilter::CompareThreshold = 1.0f;

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
	createResult("changeset");
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
		SweepLineTransformation<float> comparison(ahnFiles, _results[0].path.string(), nullptr, _progress);
		setIntermediateFormat(comparison);
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
			if (std::abs(difference) >= 1000 || std::abs(difference) <= CompareThreshold)
				difference = static_cast<float>(comparison.nodataValue);
			return difference;
		};
		comparison.nodataValue = 0;

		comparison.execute();
		_results[0].dataset = comparison.target();
	}
	else
	{
		std::vector<GByte> buffer((
			std::istreambuf_iterator<char>(std::cin)),
			(std::istreambuf_iterator<char>()));
		VSILFILE* vsiFile = VSIFileFromMemBuffer(StreamInputFile.c_str(), &buffer[0], buffer.size(), false);

		// Creating changeset
		SweepLineTransformation<float> comparison({ StreamInputFile, StreamInputFile }, _results[0].path.string(), nullptr, _progress);
		setIntermediateFormat(comparison);
		comparison.computation = [&comparison]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& ahn2 = sources[0];
			const Window<float>& ahn3 = sources[1];

			float difference = ahn3.data() - ahn2.data();
			if (std::abs(difference) >= 1000 || std::abs(difference) <= CompareThreshold)
				difference = static_cast<float>(comparison.nodataValue);
			return difference;
		};
		comparison.nodataValue = 0;

		comparison.execute();
		_results[0].dataset = comparison.target();
		VSIFCloseL(vsiFile);
		VSIUnlink(StreamInputFile.c_str());
	}

	// Noise filtering
	_progressMessage = "Noise filtering";
	createResult("noise");
	{
		int range = 2;
		SweepLineTransformation<float> noiseFilter({ _results[0].dataset }, _results[1].path.string(), range, nullptr, _progress);
		setIntermediateFormat(noiseFilter);
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
		_results[1].dataset = noiseFilter.target();
	}
	deleteResult(); // basic changeset

	// Binarization
	_progressMessage = "Sieve filtering / prepare";
	createResult("sieve");
	{
		SweepLineTransformation<GByte, float> binarization({ _results[0].dataset }, _results[1].path.string(), 0, nullptr, _progress);
		setIntermediateFormat(binarization);
		binarization.computation = [&binarization]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];
			if (!source.hasData()) return 1;
			else return 255;

		};
		binarization.nodataValue = 0;

		binarization.execute();
		_results[1].dataset = binarization.target();
	}

	// Sieve filtering
	_progressMessage = "Sieve filtering / execute";
	{
		GDALRasterBand* band = _results[1].dataset->GetRasterBand(1);

		GDALSieveFilter(
			band, nullptr, band,
			400, 4,
			nullptr, gdalProgress, static_cast<void*>(this));
	}

	// Cluster filtering
	_progressMessage = "Cluster filtering";
	createResult("cluster");
	{
		std::vector<GDALDataset*> sources = { _results[0].dataset, _results[1].dataset };
		SweepLineTransformation<float> clusterFilter(sources, _results[2].path.string(), 0, nullptr, _progress);
		setIntermediateFormat(clusterFilter);
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
		_results[2].dataset = clusterFilter.target();
	}
	deleteResult(); // noise filter
	deleteResult(); // sieve filter

	// Majority filtering
	for (int range = 1; range <= 2; ++range)
	{
		_progressMessage = "Majority filtering / r=" + std::to_string(range);
		createResult("majority");
		{
			SweepLineTransformation<float> majorityFilter({ _results[0].dataset }, _results[1].path.string(), range, nullptr, _progress);
			setIntermediateFormat(majorityFilter);
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
			majorityFilter.nodataValue = 0;

			majorityFilter.execute();
			_results[1].dataset = majorityFilter.target();
		}
		deleteResult(); // cluster filter or
						// previous majority filter
	}

	// Write out the results
	_progressMessage = "Writing results";
	createResult(std::string(), true);
	{
		// GTiff creation options
		char **params = nullptr;
		params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

		// Copy results to disk or VSI
		GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
		_results[1].dataset = driver->CreateCopy(_results[1].path.string().c_str(), _results[0].dataset,
		                                        false, params, gdalProgress, static_cast<void*>(this));
		CSLDestroy(params);

		// Stream the output
		if (hasFlag(_mode, IOMode::Stream))
		{
			vsi_l_offset length;
			GByte* buffer = VSIGetMemFileBuffer(_results[1].path.string().c_str(), &length, true);
			std::copy(buffer, buffer + length, std::ostream_iterator<GByte>(std::cout));
			delete[] buffer;
		}
	}

	// No color relief
	if (colorFile.empty() || !fs::is_regular_file(colorFile) || hasFlag(_mode, IOMode::Stream))
	{
		deleteResult(); // last majority filter
		deleteResult(); // building filter result
		return;
	}

	// Color relief
	_progressMessage = "Color relief";
	createResult("rgb", true);
	{
		char **params = nullptr;
		params = CSLAddString(params, "-alpha");
		params = CSLAddString(params, "-co");
		params = CSLSetNameValue(params, "COMPRESS", "DEFLATE");

		GDALDEMProcessingOptions *options = GDALDEMProcessingOptionsNew(params, nullptr);
		GDALDEMProcessingOptionsSetProgress(options, gdalProgress, static_cast<void*>(this));

		_results[2].dataset = static_cast<GDALDataset*>(
			GDALDEMProcessing(
				_results[2].path.string().c_str(), _results[0].dataset,
				"color-relief", colorFile.string().c_str(),
				options, nullptr)
			);
		GDALDEMProcessingOptionsFree(options);
		CSLDestroy(params);
	}

	deleteResult(); // last majority filter
	deleteResult(); // building filter result
	deleteResult(); // color relief
}

void BuildingFilter::setIntermediateFormat(Transformation& transformation) const
{
	transformation.targetFormat = hasFlag(_mode, IOMode::Files) ? "GTiff" : "MEM";
	if (transformation.targetFormat == "GTiff")
		transformation.createOptions.insert(std::make_pair("COMPRESS", "DEFLATE"));
}

unsigned int BuildingFilter::createResult(std::string suffix, bool isFinal)
{
	ResultFile result(isFinal || debug);

	// Physical disk
	if(!isFinal && hasFlag(_mode, IOMode::Files) ||
		isFinal && !hasFlag(_mode, IOMode::Stream))
	{
		std::string filename = tileName;
		if (!isFinal)
			filename += "_" + std::to_string(_nextResult++);
		if (suffix.length() > 0)
			filename += "_" + suffix;
		filename += ".tif";

		result.path = _outputDir / filename;
	}
	// Virtual file
	else if(isFinal && hasFlag(_mode, IOMode::Stream))
	{
		std::string filename = tileName;
		if (!isFinal)
			filename += "_" + std::to_string(_nextResult++);
		if (suffix.length() > 0)
			filename += "_" + suffix;
		filename += ".tif";

		result.path = fs::path("/vsimem/") / filename;
	}
	// In-memory
	else
	{
		// DO NOTHING
	}
	_results.push_back(std::move(result));
	return _results.size() - 1;
}

void BuildingFilter::deleteResult()
{
	if (_results.size() == 0)
		throw std::underflow_error("No result to pop.");
	_results.pop_front();
}

int BuildingFilter::gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg)
{
	BuildingFilter *filter = static_cast<BuildingFilter*>(pProgressArg);
	return filter->progress(static_cast<float>(dfComplete), filter->_progressMessage);
}
} // AHN