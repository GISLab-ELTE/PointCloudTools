#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <functional>
#include <iterator>
#include <cmath>
#include <ctime>

#ifdef _MSC_VER
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#endif

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
	fs::path consolePath = "/vsimem/input.tif"; // streaming input path

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("tile-name", po::value<std::string>(&tileName), "tile name (e.g. 37en1)")
		("ahn2-dir", po::value<fs::path>(&ahn2Dir), 
			"AHN-2 directory path\n"
			"expected: Arc/Info Binary Grid directory structure")
		("ahn3-dir", po::value<fs::path>(&ahn3Dir), 
			"AHN-3 directory path\n"
			"expected: GTiff file")
		("output-dir", po::value<fs::path>(&outputDir)->default_value(outputDir), 
			"result directory path")
		("streaming", 
			"stream input and output from and to standard I/O\n"
			"expected: double banded GTiff file\n"
			"implies use-memory flag")
		("in-memory", "keep intermediate results in memory")
		("debug", "keep intermediate results on disk")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Post-processing arguments
	std::string tileGroup = tileName.substr(0, 3);
	bool useStreaming = vm.count("streaming");
	bool useVSI = vm.count("in-memory") || vm.count("streaming");

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares DEMs of same area to retrieve differences." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!(vm.count("streaming") ^ vm.count("tile-name")))
	{
		std::cerr << "Set tile name or use streaming mode." << std::endl;
		argumentError = true;
	}
	if (!(vm.count("streaming") ^ vm.count("ahn2-dir")))
	{
		std::cerr << "Set AHN-2 input directory or use streaming mode." << std::endl;
		argumentError = true;
	}
	if (!(vm.count("streaming") ^ vm.count("ahn3-dir")))
	{
		std::cerr << "Set AHN-3 input directory or use streaming mode." << std::endl;
		argumentError = true;
	}
	if (!(vm.count("streaming") ^ vm.count("output-dir")))
	{
		std::cerr << "Set output directory or use streaming mode." << std::endl;
		argumentError = true;
	}

	if (!useStreaming && 
		(!fs::is_directory(ahn2Dir) || !fs::is_directory(ahn3Dir)))
	{
		std::cerr << "An input directory does not exists." << std::endl;
		argumentError = true;
	}

	if (!useStreaming && 
		(fs::exists(outputDir) && !fs::is_directory(outputDir)))
	{
		std::cerr << "The given output path exists but not a directory." << std::endl;
		argumentError = true;
	}
	else if (!useStreaming && 
		(!fs::exists(outputDir) && !fs::create_directory(outputDir)))
	{
		std::cerr << "Failed to create output directory." << std::endl;
		argumentError = true;
	}

	if (useVSI && vm.count("debug"))
	{
		std::cerr << "WARNING: debug mode has no effect with in-memory intermediate results." << std::endl;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Define output paths
	fs::path interChangeset, interNoise, interSieve, interCluster, interMajority;
	if (!useVSI)
	{
		interChangeset = outputDir / (tileName + "_1-diff.tif");
		interNoise = outputDir / (tileName + "_2-noise.tif");
		interSieve = outputDir / (tileName + "_3-sieve.tif");
		interCluster = outputDir / (tileName + "_4-cluster.tif");
		interMajority = outputDir / (tileName + "_5-majority.tif");
	}
	else
	{
		interChangeset = "/vsimem/diff.tif";
		interNoise = "/vsimem/noise.tif";
		interSieve = "/vsimem/sieve.tif";
		interCluster = "/vsimem/cluster.tif";
		interMajority = outputDir / (tileName + "_majority.tif");
	}


	// Program
	std::ofstream nullStream;
	std::ostream &out = !useStreaming ? std::cout : nullStream;
	Reporter *reporter = new BarReporter();	

	out << "=== AHN Building ===" << std::endl;
	std::clock_t clockStart = std::clock();
	GDALAllRegister();

	// Create basic changeset
	if (!useStreaming)
	{
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

		std::vector<std::string> ahnFiles(ahn2Files.size() + 1);
		ahnFiles[0] = ahn3File.string();
		std::transform(ahn2Files.begin(), ahn2Files.end(), ahnFiles.begin() + 1,
			[](fs::path ahn2File)
		{
			return ahn2File.string();
		});

		// Creating changeset
		out << "Task: Creating changeset" << std::endl
			<< "Path: " << interChangeset << std::endl;
		SweepLine<float> comparison(ahnFiles, interChangeset.string(), nullptr);
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
		out << std::endl;
	}
	else
	{
		#ifdef _MSC_VER
		_setmode(_fileno(stdin), _O_BINARY);
		#endif

		std::vector<GByte> buffer((
			std::istreambuf_iterator<char>(std::cin)),
			(std::istreambuf_iterator<char>()));
		VSILFILE* vsiFile = VSIFileFromMemBuffer(consolePath.string().c_str(), &buffer[0], buffer.size(), false);

		// Creating changeset
		out << "Task: Creating changeset" << std::endl
			<< "Path: " << interChangeset << std::endl;
		SweepLine<float> comparison({ consolePath.string(), consolePath.string() }, interChangeset.string(), nullptr);
		comparison.computation = [&comparison]
		(int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& ahn2 = sources[0];
			const Window<float>& ahn3 = sources[1];

			float difference = ahn3.data() - ahn2.data();
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
		out << std::endl;
		VSIFCloseL(vsiFile);
	}
	
	// Noise filtering
	out << "Task: Noise filtering" << std::endl
		<< "Path: " << interNoise << std::endl;
	{
		int range = 2;
		SweepLine<float> noiseFilter({ interChangeset.string() }, interNoise.string(), range, nullptr);
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
		out << std::endl;
	}
	if (!useVSI && !vm.count("debug")) fs::remove(interChangeset);
	else if(useVSI) VSIUnlink(interChangeset.string().c_str());

	// Binarization
	out << "Task: Sieve filtering / prepare" << std::endl
		<< "Path: " << interSieve << std::endl;
	{
		SweepLine<GByte, float> binarization({ interNoise.string() }, interSieve.string(), nullptr);
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
		out << std::endl;
	}

	// Sieve filtering
	out << "Task: Sieve filtering / execute" << std::endl
		<< "Path: " << interSieve << std::endl;
	{
		GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(interSieve.string().c_str(), GA_Update));
		GDALRasterBand* band = dataset->GetRasterBand(1);

		reporter->reset();
		GDALSieveFilter(
			band, nullptr, band,
			400, 4,
			nullptr, gdalProgress, reporter);

		GDALClose(dataset);
		out << std::endl;
	}

	// Cluster filtering
	out << "Task: Cluster filtering" << std::endl
		<< "Path: " << interCluster << std::endl;
	{
		SweepLine<float> clusterFilter({ interNoise.string(), interSieve.string() }, interCluster.string(), nullptr);
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
		out << std::endl;
	}
	if (!useVSI && !vm.count("debug"))
	{
		fs::remove(interNoise);
		fs::remove(interSieve);
	}
	else if (useVSI)
	{
		VSIUnlink(interNoise.string().c_str());
		VSIUnlink(interSieve.string().c_str());
	}

	// Majority filtering
	out << "Task: Majority filtering" << std::endl
		<< "Path: " << interMajority << std::endl;
	{
		int range = 1;
		SweepLine<float> majorityFilter({ interCluster.string() }, interMajority.string(), range, nullptr);
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
		out << std::endl;
	}
	if (!useVSI && !vm.count("debug")) fs::remove(interCluster);
	else if(useVSI) VSIUnlink(interCluster.string().c_str());

	// Finalization
	std::clock_t clockEnd = std::clock();
	out << "All completed!" << std::endl
		<< std::fixed << std::setprecision(2) << "CPU time used: "
		<< 1.f * (clockEnd - clockStart) / CLOCKS_PER_SEC << "s" << std::endl;
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
