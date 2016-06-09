#include <iostream>
#include <iomanip>
#include <thread>
#include <future>
#include <chrono>
#include <unordered_map>
#include <ctime>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <CloudTools.Common/IO.h>
#include <AHN.Buildings/BuildingFilter.h>
#include <AHN.Buildings/BuildingFilter.cpp>


namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace std::chrono_literals;
using namespace CloudTools::IO;
using namespace AHN;

int main(int argc, char* argv[]) try
{
	fs::path ahn2Dir;
	fs::path ahn3Dir;
	fs::path outputDir = fs::current_path();
	std::string pattern = "[[:digit:]]{2}[[:alpha:]]{2}[[:digit:]]";
	short jobCount = std::thread::hardware_concurrency();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn2-dir", po::value<fs::path>(&ahn2Dir),
			"AHN-2 directory path\n"
			"expected: Arc/Info Binary Grid directory structure")
		("ahn3-dir", po::value<fs::path>(&ahn3Dir),
			"AHN-3 directory path\n"
			"expected: GTiff file")
		("output-dir", po::value<fs::path>(&outputDir)->default_value(outputDir),
			"result directory path")
		("pattern", po::value<std::string>(&pattern)->default_value(pattern),
			"tile name pattern")
		("jobs,j", po::value<short>(&jobCount)->default_value(jobCount),
			"number of jobs to execute simultaneously")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares pairs of AHN-2 and AHN-3 tiles parallely and filters out changes in buildings." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("ahn2-dir") || !vm.count("ahn3-dir"))
	{
		std::cerr << "Input directories are mandatory." << std::endl;
		argumentError = true;
	}

	if (!fs::is_directory(ahn2Dir) || !fs::is_directory(ahn3Dir))
	{
		std::cerr << "An input directory does not exists." << std::endl;
		argumentError = true;
	}

	if (fs::exists(outputDir) && !fs::is_directory(outputDir))
	{
		std::cerr << "The given output path exists but not a directory." << std::endl;
		argumentError = true;
	}
	else if (!fs::exists(outputDir) && !fs::create_directory(outputDir))
	{
		std::cerr << "Failed to create output directory." << std::endl;
		argumentError = true;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	std::cout << "=== AHN Building Parallel ===" << std::endl;
	std::clock_t clockStart = std::clock();

	// Parallel process of tiles
	std::unordered_map<std::string, std::future<void>> futures;
	futures.reserve(jobCount);
	for(fs::directory_iterator file(ahn3Dir); file != fs::directory_iterator(); ++file)
	{
		if (fs::is_regular_file(file->status()) && file->path().extension() == ".tif")
		{
			boost::regex tilePattern(pattern);
			boost::smatch tileName;

			if (boost::regex_search(file->path().string(), tileName, tilePattern))
			{
				if (!fs::exists(outputDir / (tileName.str() + ".tif")))
				{
					std::string tile = tileName.str();
					futures.insert(
					{
						tile,
						std::async(std::launch::async, buildingFilter, tile, ahn2Dir, ahn3Dir, outputDir, IOMode::Memory, false, true)
					});
					std::cout << "Tile '" << tile << "' is starting." << std::endl;
					std::this_thread::sleep_for(20s);
				}
				else
					std::cout << "Tile '" << tileName << "' skipped." << std::endl;
			}

			if (futures.size() == jobCount)
			{
				for (auto& pair : futures)
				{
					pair.second.wait();
					std::cout << "Tile '" << pair.first << "' is ready." << std::endl;
				}
				futures.clear();
				futures.reserve(jobCount);
			}
		}
	}
	for (auto& pair : futures)
	{
		pair.second.wait();
		std::cout << "Tile '" << pair.first << "' is ready." << std::endl;
	}

	// Finalization
	std::clock_t clockEnd = std::clock();
	std::cout << "All completed!" << std::endl
		<< std::fixed << std::setprecision(2) << "CPU time used: "
		<< 1.f * (clockEnd - clockStart) / CLOCKS_PER_SEC << "s" << std::endl;

	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
