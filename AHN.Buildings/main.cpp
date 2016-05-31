#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal_priv.h>
#include <gdal_alg.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include <CloudLib.DEM/SweepLine.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudLib::DEM;
using namespace CloudTools::IO;

int CPL_STDCALL gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg);

int main(int argc, char* argv[])
{
	std::string tileName = "37en1";
	fs::path ahn2Dir;
	fs::path ahn3Dir;
	fs::path outputDir = fs::current_path().append("out.tif").string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("tile-name", po::value<std::string>(&tileName), "tile name (e.g. 37en1)")
		("ahn2-dir", po::value<fs::path>(&ahn2Dir), "AHN-2 directory path")
		("ahn3-dir", po::value<fs::path>(&ahn3Dir), "AHN-3 directory path")
		("output-dir", po::value<fs::path>(&outputDir)->default_value(outputDir), "result directory path")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Post-processing arguments
	std::string tileGroup = tileName.substr(0, 3);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares DEMs of same area to retrieve differences." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	if (!vm.count("tile-name") || !vm.count("ahn2-dir") || !vm.count("ahn3-dir") || !vm.count("output-dir"))
	{
		std::cerr << "Mandatory arguments: tile-name, ahn2-dir, ahn3-dir." << std::endl;
		return InvalidInput;
	}	

	if (!fs::is_directory(ahn2Dir) || !fs::is_directory(ahn3Dir))
	{
		std::cerr << "An input directory does not exists." << std::endl;
		return InvalidInput;
	}

	if (fs::exists(outputDir) && !fs::is_directory(outputDir))
	{
		std::cerr << "The given output path is not a directory." << std::endl;
		return InvalidInput;
	}
	else if (!fs::exists(outputDir) && !fs::create_directory(outputDir))
	{
		std::cerr << "The output directory was failed to create." << std::endl;
		return InvalidInput;
	}


	// Program
	std::cout << "=== AHN Building ===" << std::endl;
	Reporter *reporter = new BarReporter();
	GDALAllRegister();

	// Collect input files
	std::vector<fs::path> ahn2Files;
	for (fs::directory_iterator itGroup(ahn2Dir); itGroup != fs::directory_iterator(); ++itGroup)
	{
		if (fs::is_directory(itGroup->path()) &&
			itGroup->path().filename().string().find(tileGroup) != std::string::npos)
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
	for (fs::directory_iterator itTile(ahn3Dir); itTile != fs::directory_iterator(); ++itTile)
	{
		if (fs::is_regular_file(itTile->path()) &&
			itTile->path().filename().string().find(tileName) != std::string::npos)
		{
			ahn3File = itTile->path();
			break;
		}
	}	

	// Define output paths
	fs::path resultChangeset = outputDir / (tileName + "_1-diff.tif");
	fs::path resultNoise = outputDir / (tileName + "_2-noise.tif");
	fs::path resultSieve = outputDir / (tileName + "_3-sieve.tif");
	fs::path resultCluster = outputDir / (tileName + "_4-cluster.tif");
	fs::path resultMajority = outputDir / (tileName + "_5-majority.tif");

	// Create basic changeset
	int step = 1;
	std::cout << step++ << ". Creating changeset" << std::endl
		<< "Path: " << resultChangeset << std::endl;
	{
		std::vector<std::string> ahnFiles(ahn2Files.size() + 1);
		ahnFiles[0] = ahn3File.string();
		std::transform(ahn2Files.begin(), ahn2Files.end(), ahnFiles.begin() + 1, 
			[](fs::path ahn2File)
		{
			return ahn2File.string();
		});

		SweepLine<float> comparison(ahnFiles, resultChangeset.string(), nullptr);
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
			if (std::abs(difference) >= 1000 || std::abs(difference) <= 0.2)
				difference = static_cast<float>(comparison.nodataValue);
			return difference;
		};
		comparison.nodataValue = 0;
		comparison.progress = [&reporter](float complete, std::string message)
		{
			reporter->report(complete, message);
			return true;
		};

		reporter->reset();
		comparison.execute();
		std::cout << std::endl;
	}
	
	// Noise filtering
	std::cout << step++ << ". Noise filtering" << std::endl
		<< "Path: " << resultNoise << std::endl;
	{
		int range = 2;
		SweepLine<float> noiseFilter({ resultChangeset.string() }, resultNoise.string(), range, nullptr);
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
		noiseFilter.progress = [&reporter](float complete, std::string message)
		{
			reporter->report(complete, message);
			return true;
		};

		reporter->reset();
		noiseFilter.execute();
		std::cout << std::endl;
	}

	// Binarization
	std::cout << step++ << ". Binarization" << std::endl
		<< "Path: " << resultSieve << std::endl;
	{
		SweepLine<GByte, float> binarization({ resultNoise.string() }, resultSieve.string(), nullptr);
		binarization.computation = [&binarization]
			(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];
			if (!source.hasData()) return 1;
			else return 255;

		};
		binarization.nodataValue = 0;
		binarization.progress = [&reporter](float complete, std::string message)
		{
			reporter->report(complete, message);
			return true;
		};

		reporter->reset();
		binarization.execute();
		std::cout << std::endl;
	}

	// Sieve filtering
	std::cout << step++ << ". Sieve filtering" << std::endl
		<< "Path: " << resultSieve << std::endl;
	{
		GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(resultSieve.string().c_str(), GA_Update));
		GDALRasterBand* band = dataset->GetRasterBand(1);

		reporter->reset();
		GDALSieveFilter(
			band, nullptr, band,
			400, 4,
			nullptr, gdalProgress, reporter);

		GDALClose(dataset);
		std::cout << std::endl;
	}

	// Cluster filtering
	std::cout << step++ << ". Cluster filtering" << std::endl
		<< "Path: " << resultCluster << std::endl;
	{
		SweepLine<float> clusterFilter({ resultNoise.string(), resultSieve.string() }, resultCluster.string(), nullptr);
		clusterFilter.computation = [&clusterFilter]
			(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& noise = sources[0];
			const Window<float>& sieve = sources[1];

			if (sieve.hasData() && sieve.data() == 255) return noise.data();
			else return static_cast<float>(clusterFilter.nodataValue);

		};
		clusterFilter.nodataValue = 0;
		clusterFilter.progress = [&reporter](float complete, std::string message)
		{
			reporter->report(complete, message);
			return true;
		};

		reporter->reset();
		clusterFilter.execute();
		std::cout << std::endl;
	}

	// Majority filtering
	std::cout << step++ << ". Majority filtering" << std::endl
		<< "Path: " << resultCluster << std::endl;
	{
		int range = 1;
		SweepLine<float> majorityFilter({ resultCluster.string() }, resultMajority.string(), range, nullptr);
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
		majorityFilter.progress = [&reporter](float complete, std::string message)
		{
			reporter->report(complete, message);
			return true;
		};

		reporter->reset();
		majorityFilter.execute();
		std::cout << std::endl;
	}

	std::cout << "All completed!" << std::endl;
	return Success;
}

/// <summary>
/// Routes the C-style GDAL progress reports to the defined reporter.
/// </summary>
int CPL_STDCALL gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg)
{
	Reporter *reporter = static_cast<Reporter*>(pProgressArg);
	reporter->report(static_cast<float>(dfComplete), std::string());
	return true;
}
