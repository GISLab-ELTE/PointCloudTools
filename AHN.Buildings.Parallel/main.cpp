#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <ctime>
#include <stdexcept>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <AHN.Buildings/Process.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace AHN::Buildings;

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
/// Looks for the given tile input file in the specified directory.
/// </summary>
/// <param name="directory">Directory path.</param>
/// <param name="tileName">Name of the tile.</param>
/// <returns>Path for the found entry or exception is thrown when non found.</returns>
fs::path lookupFile(const std::string& directory, const std::string& tileName);

/// <summary>
/// Processes a tile.
/// </summary>
/// <param name="tileName">Name of the tile.</param>
/// <param name="ahn2Surface">AHN-2 surface DEM directory path.</param>
/// <param name="ahn3Surface">AHN-3 surface DEM directory path.</param>
/// <param name="ahn2Terrain">AHN-2 terrain DEM directory path.</param>
/// <param name="ahn3Terrain">AHN-3 terrain DEM directory path.</param>
/// <param name="outputDir">Result directory path.</param>
void processTile(const std::string& tileName,
                 const std::string& ahn2Surface, const std::string& ahn3Surface,
                 const std::string& ahn2Terrain, const std::string& ahn3Terrain,
                 const std::string& outputDir, const std::string& colorFile);

int main(int argc, char* argv[]) try
{
	std::string ahn2SurfaceDir,
	            ahn3SurfaceDir,
	            ahn2TerrainDir,
	            ahn3TerrainDir;
	std::string outputDir = fs::current_path().string();
	std::string colorFile;
	std::string pattern = "[[:digit:]]{2}[[:alpha:]]{2}[[:digit:]]";
	unsigned short maxJobs = std::thread::hardware_concurrency();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn2-surface", po::value<std::string>(&ahn2SurfaceDir),
		 "AHN-2 surface DEM directory path")
		("ahn3-surface", po::value<std::string>(&ahn3SurfaceDir),
		 "AHN-3 surface DEM directory path")
		("ahn2-terrain", po::value<std::string>(&ahn2TerrainDir),
		 "AHN-2 terrain DEM directory path")
		("ahn3-terrain", po::value<std::string>(&ahn3TerrainDir),
		 "AHN-3 terrain DEM directory path")
		("output-dir", po::value<std::string>(&outputDir)->default_value(outputDir),
		 "result directory path")
		("pattern", po::value<std::string>(&pattern)->default_value(pattern),
		 "tile name pattern")
		("color-file", po::value<std::string>(&colorFile),
		 "map file for color relief; see:\n"
		 "http://www.gdal.org/gdaldem.html")
		("jobs,j", po::value<unsigned short>(&maxJobs)->default_value(maxJobs),
		 "number of maximum jobs to execute simultaneously")
		("help,h", "produce help message");

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
	if (!vm.count("ahn2-surface") || !vm.count("ahn3-surface"))
	{
		std::cerr << "Surface input directories are mandatory." << std::endl;
		argumentError = true;
	}

	if (!fs::is_directory(ahn2SurfaceDir) || !fs::is_directory(ahn3SurfaceDir))
	{
		std::cerr << "A surface input directory does not exist." << std::endl;
		argumentError = true;
	}

	if (vm.count("ahn2-terrain") != vm.count("ahn3-terrain"))
	{
		std::cerr << "Only one of the terrain DEM directories was given." << std::endl;
		argumentError = true;
	}

	if (vm.count("ahn2-terrain") && vm.count("ahn3-terrain") &&
		(!fs::is_directory(ahn2TerrainDir) || !fs::is_directory(ahn3TerrainDir)))
	{
		std::cerr << "A terrain input directory does not exist." << std::endl;
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

	for (fs::directory_iterator item(ahn3SurfaceDir); item != fs::directory_iterator(); ++item)
	{
		if (fs::is_regular_file(item->status()) && item->path().extension() == ".tif")
		{
			boost::regex tilePattern(pattern);
			boost::smatch tileMatch;

			if (boost::regex_search(item->path().filename().string(), tileMatch, tilePattern))
			{
				std::string tileName = tileMatch.str();
				std::string ahn3SurfaceFile,
				            ahn2SurfaceFile,
				            ahn3TerrainFile,
				            ahn2TerrainFile;

				try
				{
					ahn3SurfaceFile = item->path().string();
					ahn2SurfaceFile = lookupFile(ahn2SurfaceDir, tileName).string();
				}
				catch (std::exception&)
				{
					std::cerr << "WARNING: skipped tile '" << tileName << "' because not all surface DEM files were present." << std::endl;
					continue;
				}
				if (vm.count("ahn2-terrain") && vm.count("ahn3-terrain"))
				{
					try
					{
						ahn3TerrainFile = lookupFile(ahn3TerrainDir, tileName).string();
						ahn2TerrainFile = lookupFile(ahn2TerrainDir, tileName).string();
					}
					catch (std::exception&)
					{
						std::cerr << "WARNING: skipped tile '" << tileName << "' because not all terrain DEM files were present." << std::endl;
						continue;
					}
				}

				// New tile process can be started if none is under initialization and there is a free job slot.
				std::unique_lock<std::mutex> lock(initMutex);
				initCondition.wait(lock,
				                   [maxJobs, &futures]
				                   {
					                   if (!isInitializing && futures.size() > 0)
					                   {
						                   std::cout << std::endl << "Job status:" << std::endl;
						                   for (auto it = futures.begin(); it != futures.end();)
						                   {
											   const auto item = it++;
							                   std::cout << "Tile '" << item->first << "': ";
							                   if (item->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
							                   {
								                   std::cout << "ready" << std::endl;
								                   futures.erase(item);
							                   }
							                   else
								                   std::cout << "processing" << std::endl;
						                   };
						                   if (futures.size() == maxJobs)
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
					               std::async(std::launch::async,
					                          processTile, tileName,
					                          ahn2SurfaceFile, ahn3SurfaceFile,
					                          ahn2TerrainFile, ahn3TerrainFile,
					                          outputDir, colorFile)));
			}
		}
	}

	// Waiting for remaining jobs to finish.
	std::unique_lock<std::mutex> lock(initMutex);
	initCondition.wait(lock, [maxJobs]
	                   {
		                   return !isInitializing;
	                   });
	lock.unlock();

	std::cout << std::endl
		<< "All jobs started." << std::endl
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
catch (std::exception& ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}

fs::path lookupFile(const std::string& directory, const std::string& tileName)
{
	fs::directory_iterator result = std::find_if(fs::directory_iterator(directory), fs::directory_iterator(),
		[&tileName](fs::directory_entry entry)
	{
		if (!fs::is_regular_file(entry.status()) || entry.path().extension() != ".tif")
			return false;

		boost::regex pattern(".*" + tileName + ".*");
		return boost::regex_match(entry.path().stem().string(), pattern);
	});

	if (result == fs::directory_iterator())
		throw std::runtime_error("No input found in directory '" + directory + "' for tile '" + tileName + "'.");
	return result->path();
}

void processTile(const std::string& tileName,
                 const std::string& ahn2Surface, const std::string& ahn3Surface,
                 const std::string& ahn2Terrain, const std::string& ahn3Terrain,
                 const std::string& outputDir, const std::string& colorFile)
{
	// Variable for the detection of the end of initialization
	BarReporter reporter;
	std::string lastStatus;
	unsigned int statusNumber = 0;
	bool isInitialized = false;

	// Process configuration
	InMemoryProcess* process;
	if (ahn2Terrain.empty() || ahn3Terrain.empty())
		process = new InMemoryProcess(tileName, ahn2Surface, ahn3Surface, outputDir);
	else
		process = new InMemoryProcess(tileName, ahn2Surface, ahn3Surface, ahn2Terrain, ahn3Terrain, outputDir);
	process->progress = [&reporter, &lastStatus, &statusNumber, &isInitialized]
		(float complete, const std::string &message)
		{
			if (message != lastStatus)
			{
				lastStatus = message;
				++statusNumber;
			}

			// The first 3 statuses are the initialization
			if (statusNumber < 4)
				reporter.report(complete / 3 + (statusNumber - 1) / 3.f, message);
			else if (!isInitialized && statusNumber == 4)
			{
				reporter.report(1.f);
				isInitialized = true;
				std::lock_guard<std::mutex> lock(initMutex);
				isInitializing = false;
				initCondition.notify_all();
			}
			return true;
		};
	process->colorFile = colorFile;

	// Execute process
	try
	{
		process->execute();
	}
	catch(std::exception& ex)
	{
		std::cerr << "ERROR processing tile '" << tileName << "' " << std::endl
			      << "ERROR: " << ex.what() << std::endl;

		if (!isInitialized)
		{
			isInitialized = true;
			std::lock_guard<std::mutex> lock(initMutex);
			isInitializing = false;
		}
	}
	delete process;

	initCondition.notify_all();
}
