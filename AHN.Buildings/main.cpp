#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include "IOMode.h"
#include "Process.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace AHN::Buildings;

int main(int argc, char* argv[]) try
{
	std::string tileName;
	std::string ahn2Surface,
	            ahn3Surface,
	            ahn2Terrain,
	            ahn3Terrain;
	std::string outputDir = fs::current_path().string();
	std::string colorFile;
	IOMode mode = IOMode::Files;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("tile-name", po::value<std::string>(&tileName), "tile name (e.g. 37en1)")
		("ahn2-surface", po::value<std::string>(&ahn2Surface),
			"AHN-2 surface DEM file path")
		("ahn3-surface", po::value<std::string>(&ahn3Surface),
			"AHN-3 surface DEM file path")
		("ahn2-terrain", po::value<std::string>(&ahn2Terrain),
			"AHN-2 terrain DEM file path")
		("ahn3-terrain", po::value<std::string>(&ahn3Terrain),
			"AHN-3 terrain DEM file path")
		("output-dir", po::value<std::string>(&outputDir)->default_value(outputDir),
			"result directory path")
		("color-file", po::value<std::string>(&colorFile),
			"map file for color relief\n"
			"ignored when streaming")
		("mode,m", po::value<IOMode>(&mode)->default_value(mode),
			"I/O mode, supported\n"
			"FILES, MEMORY, STREAM, HADOOP")
		("debug,d", "keep intermediate results on disk after progress\n"
					"applies only to FILES mode")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares an AHN-2 and AHN-3 tile pair and filters out changes in buildings." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!hasFlag(mode, IOMode::Stream))
	{
		if (!vm.count("ahn2-surface") || !vm.count("ahn3-surface"))
		{
			std::cerr << "Surface input files are mandatory when not using streaming mode." << std::endl;
			argumentError = true;
		}

		if (!fs::exists(ahn2Surface) || !fs::exists(ahn3Surface))
		{
			std::cerr << "A surface input file does not exist." << std::endl;
			argumentError = true;
		}

		if (vm.count("ahn2-terrain") != vm.count("ahn3-terrain"))
		{
			std::cerr << "Only one of the terrain DEM files was given." << std::endl;
			argumentError = true;
		}

		if (vm.count("ahn2-terrain") && vm.count("ahn3-terrain") &&
			(!fs::exists(ahn2Terrain) || !fs::exists(ahn3Terrain)))
		{
			std::cerr << "A terrain input file does not exist." << std::endl;
			argumentError = true;
		}

		if (fs::exists(outputDir) && !fs::is_directory(outputDir))
		{
			std::cerr << "The given output path exists but is not a directory." << std::endl;
			argumentError = true;
		}
		else if (!fs::exists(outputDir) && !fs::create_directory(outputDir))
		{
			std::cerr << "Failed to create output directory." << std::endl;
			argumentError = true;
		}
	}

	if (!hasFlag(mode, IOMode::Hadoop))
	{
		if (!vm.count("tile-name"))
		{
			std::cerr << "Tile name is mandatory when not using Hadoop Steaming." << std::endl;
			argumentError = true;
		}
	}

	if (vm.count("color-file") && !fs::is_regular_file(colorFile))
	{
		std::cerr << "The given color file does not exist." << std::endl;
		argumentError = true;
	}

	if (hasFlag(mode, IOMode::Memory) && vm.count("debug"))
	{
		std::cerr << "WARNING: debug mode has no effect with in-memory intermediate results." << std::endl;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	std::ofstream nullStream;
	std::ostream &out = !hasFlag(mode, IOMode::Stream) ? std::cout : nullStream;
	Reporter *reporter = !hasFlag(mode, IOMode::Stream)
		? static_cast<Reporter*>(new BarReporter())
		: static_cast<Reporter*>(new NullReporter());

	if (!vm.count("quiet"))
		out << "=== AHN Building Filter ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

	// Configure the operation
	GDALAllRegister();
	Process* process;
	std::string lastStatus;

	switch (mode)
	{
	case IOMode::Files:
	{
		FileBasedProcess* typedProcess;
		if (vm.count("ahn2-terrain") && vm.count("ahn3-terrain"))
			typedProcess = new FileBasedProcess(tileName, ahn2Surface, ahn3Surface, ahn2Terrain, ahn3Terrain, outputDir);
		else
			typedProcess = new FileBasedProcess(tileName, ahn2Surface, ahn3Surface, outputDir);
		typedProcess->colorFile = colorFile;
		typedProcess->debug = vm.count("debug");
		process = typedProcess;
		break;
	}
	case IOMode::Memory:
	{
		InMemoryProcess* typedProcess;
		if (vm.count("ahn2-terrain") && vm.count("ahn3-terrain"))
			typedProcess = new InMemoryProcess(tileName, ahn2Surface, ahn3Surface, ahn2Terrain, ahn3Terrain, outputDir);
		else
			typedProcess = new InMemoryProcess(tileName, ahn2Surface, ahn3Surface, outputDir);
		typedProcess->colorFile = colorFile;
		process = typedProcess;
		break;
	}
	case IOMode::Stream:
		process = new StreamedProcess(tileName);
		break;
	case IOMode::Hadoop:
		process = new HadoopProcess();
		break;
	default:
		// Unsigned and complex types are not supported.
		std::cerr << "Unsupported I/O mode given." << std::endl;
		return Unsupported;
	}
	
	if (!vm.count("quiet"))
	{
		process->progress = [&out, &reporter, &lastStatus](float complete, const std::string &message)
		{
			if(message != lastStatus)
			{
				out << std::endl
					<< "Task: " << message << std::endl;
				reporter->reset();
				lastStatus = message;
			}
			reporter->report(complete, message);
			return true;
		};
	}

	// Execute operation
	process->execute();
	delete process;
	delete reporter;

	// Execution time measurement
	std::clock_t clockEnd = std::clock();
	auto timeEnd = std::chrono::high_resolution_clock::now();

	if (!vm.count("quiet"))
	{
		out << std::endl 
			<< "All completed!" << std::endl
			<< std::fixed << std::setprecision(2) << "CPU time used: "
			<< 1.f * (clockEnd - clockStart) / CLOCKS_PER_SEC << "s" << std::endl
			<< "Wall clock time passed: "
			<< std::chrono::duration<float>(timeEnd - timeStart).count() << "s" << std::endl;
	}
	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
