#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <ctime>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include <AHN.Buildings/BuildingFilter.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace std::chrono_literals;
using namespace CloudTools::IO;
using namespace AHN;

/// <summary>
/// Mutex for guarding the starting of a new tile process.
/// </summary>
std::mutex initMutex;
/// <summary>
/// Condition variable for guarding the starting of a new tile process.
/// </summary>
std::condition_variable initCondition;
/// <summary>
/// <c>true</c> if a tile is under initialization; otherwise <c>false</c>.
/// </summary>
bool isInitializing = false;

/// <summary>
/// Processes a tile.
/// </summary>
/// <param name="tileName">Name of the tile.</param>
/// <param name="ahn2Dir">AHN-2 directory path.</param>
/// <param name="ahn3Dir">AHN-3 directory path.</param>
/// <param name="outputDir">Result directory path.</param>
void processTile(std::string tileName, fs::path ahn2Dir, fs::path ahn3Dir, fs::path outputDir, fs::path colorFile);

int main(int argc, char* argv[]) try
{
	std::string ahn2Dir;
	std::string ahn3Dir;
	std::string outputDir = fs::current_path().string();
	std::string colorFile;
	std::string pattern = "[[:digit:]]{2}[[:alpha:]]{2}[[:digit:]]";
	short maxJobs = std::thread::hardware_concurrency();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn2-dir", po::value<std::string>(&ahn2Dir),
			"AHN-2 directory path\n"
			"expected: Arc/Info Binary Grid directory structure")
		("ahn3-dir", po::value<std::string>(&ahn3Dir),
			"AHN-3 directory path\n"
			"expected: GTiff file")
		("output-dir", po::value<std::string>(&outputDir)->default_value(outputDir),
			"result directory path")
		("pattern", po::value<std::string>(&pattern)->default_value(pattern),
			"tile name pattern")
		("color-file", po::value<std::string>(&colorFile),
			"map file for color relief; see:\n"
			"http://www.gdal.org/gdaldem.html")
		("jobs,j", po::value<short>(&maxJobs)->default_value(maxJobs),
			"number of maximum jobs to execute simultaneously")
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
		std::cerr << "An input directory does not exist." << std::endl;
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

	if (vm.count("color-file") && !fs::is_regular_file(colorFile))
	{
		std::cerr << "The given color file does not exists." << std::endl;
		argumentError = true;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	std::cout << "=== AHN Building Filter Parallel ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();
	GDALAllRegister();

	// Parallel process of tiles
	std::unordered_map<std::string, std::future<void>> futures;
	futures.reserve(maxJobs);

	for(fs::directory_iterator file(ahn3Dir); file != fs::directory_iterator(); ++file)
	{
		if (fs::is_regular_file(file->status()) && file->path().extension() == ".tif")
		{
			boost::regex tilePattern(pattern);
			boost::smatch tileMatch;

			if (boost::regex_search(file->path().string(), tileMatch, tilePattern))
			{
				std::string tileName = tileMatch.str();

				// New tile process can be started if none is under initialization and there is a free job slot.
				std::unique_lock<std::mutex>lock(initMutex);
				initCondition.wait(lock,
				               [maxJobs, &futures]
				               {
								   if (!isInitializing && futures.size() > 0)
								   {
									   std::cout << std::endl << "Job status:" << std::endl;
									   for (const auto& item : futures)
									   {
										   std::cout << "Tile '" << item.first << "': ";
										   if (item.second.wait_for(0s) == std::future_status::ready)
										   {
											   std::cout << "ready" << std::endl;
											   futures.erase(item.first);
										   }
										   else
											   std::cout << "processing" << std::endl;
									   };
									   if(futures.size() == maxJobs)
										   std::cout << "All job slots are busy, waiting." << std::endl;
								   }

					               return !isInitializing && futures.size() < maxJobs;
				               });
				isInitializing = true;
				lock.unlock();

				// Start tile processing.
				std::cout << std::endl
					<< "Tile '" << tileName << "' is being initialized ..." << std::endl;
				futures.insert(
					std::make_pair(tileName,
					               std::async(std::launch::async, processTile, tileName, ahn2Dir, ahn3Dir, outputDir, colorFile)));
			}
		}
	}

	// Waiting for remaining jobs to finish.
	std::unique_lock<std::mutex>lock(initMutex);
	initCondition.wait(lock, [maxJobs]
	{
		return !isInitializing;
	});
	lock.unlock();

	std::cout << std::endl 
		<< "All jobs started."
		<< "Waiting for remaing tasks to finish: " << std::endl;
	for (const auto& item : futures)
	{
		std::cout << "Tile '" << item.first << "': ";
		item.second.wait();
		std::cout << "ready" << std::endl;
	}
	futures.clear();

	// Execution time measurement
	std::clock_t clockEnd = std::clock();
	auto timeEnd = std::chrono::high_resolution_clock::now();

	std::cout << std::endl 
		<< "All completed!" << std::endl << std::fixed << std::setprecision(2)
		<< "CPU time used: "
		<< 1.f * (clockEnd - clockStart) / CLOCKS_PER_SEC / 60 << " min" << std::endl
		<< "Wall clock time passed: "
		<< std::chrono::duration<float>(timeEnd - timeStart).count() / 60 << " min" << std::endl;

	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}

void processTile(std::string tileName, fs::path ahn2Dir, fs::path ahn3Dir, fs::path outputDir, fs::path colorFile)
{
	BarReporter reporter;
	std::string firstStatus;
	bool isInitialized = false;

	BuildingFilter *filter = BuildingFilter::createInMemory(tileName, ahn2Dir, ahn3Dir, outputDir);
	filter->progress = [&reporter, &firstStatus, &isInitialized]
		(float complete, std::string message)
		{
			if (firstStatus.length() == 0)
				firstStatus = message;

			if (message == firstStatus)
				reporter.report(complete, message);
			else if (!isInitialized)
			{
				isInitialized = true;
				std::lock_guard<std::mutex> lock(initMutex);
				isInitializing = false;
				initCondition.notify_all();
			}
			return true;
		};
	filter->colorFile = colorFile;

	filter->execute();
	initCondition.notify_all();

	delete filter;
}
