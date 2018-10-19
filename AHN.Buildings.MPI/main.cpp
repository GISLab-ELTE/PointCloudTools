#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <stdexcept>

#include <mpi.h>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <CloudTools.Common/IO/IO.h>
#include <AHN.Buildings/Process.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace AHN::Buildings;

const std::string pattern = "[[:digit:]]{2}[[:alpha:]]{2}[[:digit:]]";

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

int main(int argc, char *argv[]) try
{
	int procCount, procId;
	std::string ahn2SurfaceDir,
		ahn3SurfaceDir,
		ahn2TerrainDir,
		ahn3TerrainDir,
		outputDir;
	std::string colorFile;

	// Initalize MPI
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &procCount);
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);

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
		("output-dir", po::value<std::string>(&outputDir),
			"result directory path")
		("color-file", po::value<std::string>(&colorFile),
			"map file for color relief; see:\n"
			"http://www.gdal.org/gdaldem.html")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares pairs of AHN-2 and AHN-3 tiles parallely (through MPI) and filters out changes in buildings." << std::endl;
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

	if (!vm.count("output-dir"))
	{
		std::cerr << "Output directory is mandatory." << std::endl;
		argumentError = true;
	}
	else if (fs::exists(outputDir) && !fs::is_directory(outputDir))
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
	std::cout << "[Process #" << procId << "] Initialization" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();
	GDALAllRegister();

	// Count input files
	int fileCount = 0;
	for (fs::directory_iterator item(ahn3SurfaceDir); item != fs::directory_iterator(); ++item)
	{
		if (fs::is_regular_file(item->status()) && item->path().extension() == ".tif")
		{
			boost::regex tilePattern(pattern);
			boost::smatch tileMatch;

			if (boost::regex_search(item->path().filename().string(), tileMatch, tilePattern))
			{
				++fileCount;
			}
		}
	}

	// Select block of files to process.
	int blockSize = fileCount / procCount;
	int fileRemainder = fileCount - procCount * blockSize;
	int blockStart = procId * blockSize + std::min(fileRemainder, procId);
	if (fileRemainder > procId)	++blockSize;
	int blockEnd = blockStart + blockSize;
	std::cout << "[Process #" << procId << "] Found " << fileCount << " files, will work on files " << blockStart << ". - " << (blockEnd - 1) << "." << std::endl;

	// Sequential process of selected tiles
	int filePassed = 0;
	for (fs::directory_iterator item(ahn3SurfaceDir); item != fs::directory_iterator(); ++item)
	{
		if (fs::is_regular_file(item->status()) && item->path().extension() == ".tif")
		{
			boost::regex tilePattern(pattern);
			boost::smatch tileMatch;

			if (boost::regex_search(item->path().filename().string(), tileMatch, tilePattern))
			{
				++filePassed;
				if (filePassed <= blockStart) continue;
				if (filePassed > blockEnd) break;

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

				// Tile processing
				std::cout << "[Process #" << procId << "] Started tile '" << tileName << "'" << std::endl;
				processTile(tileName, ahn2SurfaceFile, ahn3SurfaceFile, ahn2TerrainFile, ahn3TerrainFile, outputDir, colorFile);
				std::cout << "[Process #" << procId << "] Finished tile '" << tileName << "'" << std::endl;
			}
		}
	}

	// Execution time measurement
	std::clock_t clockEnd = std::clock();
	auto timeEnd = std::chrono::high_resolution_clock::now();

	std::cout << "[Process #" << procId << "] "
		<< std::fixed << std::setprecision(2)
		<< "CPU time used: "
		<< 1.f * (clockEnd - clockStart) / CLOCKS_PER_SEC / 60 << " min, "
		<< "Wall clock time passed: "
		<< std::chrono::duration<float>(timeEnd - timeStart).count() / 60 << " min" << std::endl;

	std::cout << "[Process #" << procId << "] Termination" << std::endl;
	MPI_Finalize();
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
	// Process configuration
	InMemoryProcess* process;
	if (ahn2Terrain.empty() || ahn3Terrain.empty())
		process = new InMemoryProcess(tileName, ahn2Surface, ahn3Surface, outputDir);
	else
		process = new InMemoryProcess(tileName, ahn2Surface, ahn3Surface, ahn2Terrain, ahn3Terrain, outputDir);
	process->progress = [](float complete, const std::string &message)
	{
		return true;
	};
	process->colorFile = colorFile;

	// Execute process
	try
	{
		process->execute();
	}
	catch (std::exception& ex)
	{
		std::cerr << "ERROR processing tile '" << tileName << "' " << std::endl
				  << "ERROR: " << ex.what() << std::endl;
	}
	delete process;
}
