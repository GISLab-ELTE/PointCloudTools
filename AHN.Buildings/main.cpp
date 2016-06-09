#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <CloudTools.Common/IO.h>
#include "IOMode.h"
#include "BuildingFilter.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace AHN;

int main(int argc, char* argv[]) try
{
	std::string tileName;
	fs::path ahn2Dir;
	fs::path ahn3Dir;
	fs::path outputDir = fs::current_path();
	IOMode mode = IOMode::Files;

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
		if (!vm.count("tile-name"))
		{
			std::cerr << "Tile name is mandatory when not using streaming mode." << std::endl;
			argumentError = true;
		}
		if (!vm.count("ahn2-dir") || !vm.count("ahn3-dir"))
		{
			std::cerr << "Input directories are mandatory when not using streaming mode." << std::endl;
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

	buildingFilter(tileName, ahn2Dir, ahn3Dir, outputDir, mode, vm.count("debug"), vm.count("quiet"));
	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
