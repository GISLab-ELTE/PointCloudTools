#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include "IOMode.h"
#include "BuildingFilter.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace AHN;

int main(int argc, char* argv[]) try
{
	std::string tileName;
	std::string ahn2Dir;
	std::string ahn3Dir;
	std::string outputDir = fs::current_path().string();
	std::string colorFile;
	IOMode mode = IOMode::Files;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("tile-name", po::value<std::string>(&tileName), "tile name (e.g. 37en1)")
		("ahn2-dir", po::value<std::string>(&ahn2Dir),
			"AHN-2 directory path\n"
			"expected: Arc/Info Binary Grid directory structure")
		("ahn3-dir", po::value<std::string>(&ahn3Dir),
			"AHN-3 directory path\n"
			"expected: GTiff file")
		("output-dir", po::value<std::string>(&outputDir)->default_value(outputDir),
			"result directory path")
		("color-file", po::value<std::string>(&colorFile),
			"map file for color relief\n"
			"ignored when streaming")
		("mode,m", po::value<IOMode>(&mode)->default_value(mode),
			"I/O mode, supported\n"
			"files, memory, stream, hadoop")
		("debug,d", "keep intermediate results on disk after progress")
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
		if (!vm.count("ahn2-dir") || !vm.count("ahn3-dir"))
		{
			std::cerr << "Input directories are mandatory when not using streaming mode." << std::endl;
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
	}

	if (!hasFlag(mode, IOMode::Hadoop))
	{
		if (!vm.count("tile-name"))
		{
			std::cerr << "Tile name is mandatory when not using Hadoop Steaming API." << std::endl;
			argumentError = true;
		}
	}

	if (vm.count("color-file") && !fs::is_regular_file(colorFile))
	{
		std::cerr << "The given color file does not exists." << std::endl;
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
	if (!vm.count("quiet"))
		std::cout << "=== AHN Building Filter ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

	BarReporter reporter;
	std::string lastStatus;

	// Configure the operation
	GDALAllRegister();
	BuildingFilter* filter;
	switch (mode)
	{
	case IOMode::Files:
		filter = BuildingFilter::createPhysical(tileName, ahn2Dir, ahn3Dir, outputDir);
		break;
	case IOMode::Memory:
		filter = BuildingFilter::createInMemory(tileName, ahn2Dir, ahn3Dir, outputDir);
		break;
	case IOMode::Stream:
		filter = BuildingFilter::createStreamed(tileName);
		break;
	case IOMode::Hadoop:
		filter = BuildingFilter::createHadoop();
		break;
	default:
		// Unsigned and complex types are not supported.
		std::cerr << "Unsupported I/O mode given." << std::endl;
		return Unsupported;
	}
	filter->colorFile = colorFile;
	filter->debug = vm.count("debug");
	if (!vm.count("quiet"))
	{
		filter->progress = [&reporter, &lastStatus](float complete, std::string message)
		{
			if(message != lastStatus)
			{
				std::cout << std::endl
					<< "Task: " << message << std::endl;
				reporter.reset();
				lastStatus = message;
			}
			reporter.report(complete, message);
			return true;
		};
	}

	// Execute operation
	filter->execute();
	delete filter;

	// Execution time measurement
	std::clock_t clockEnd = std::clock();
	auto timeEnd = std::chrono::high_resolution_clock::now();

	if (!vm.count("quiet"))
	{
		std::cout << std::endl 
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
