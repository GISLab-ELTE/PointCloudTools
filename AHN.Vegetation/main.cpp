#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>
#include <chrono>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>

#include "Process.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

int main(int argc, char* argv[])
{
	std::string DTMinputPath;
	std::string DSMinputPath;
	std::string AHN2DSMinputPath;
	std::string AHN2DTMinputPath;
	std::string outputDir = fs::current_path().string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn3-dtm-input-path,t", po::value<std::string>(&DTMinputPath), "DTM input path")
		("ahn3-dsm-input-path,s", po::value<std::string>(&DSMinputPath), "DSM input path")
		("ahn2-dtm-input-path,y", po::value<std::string>(&AHN2DTMinputPath), "AHN2 DTM input path")
		("ahn2-dsm-input-path,x", po::value<std::string>(&AHN2DSMinputPath), "AHN2 DSM input path")
		("output-dir,o", po::value<std::string>(&outputDir)->default_value(outputDir), "result directory path")
		("hausdorff-distance,d", "use Hausdorff-distance")
		("parallel,p", "parallel execution for AHN-2 and AHN-3") // TODO: this will mess up the log output
		("verbose,v", "verbose output")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares an AHN-2 and AHN-3 tile pair and filters out changes in vegetation." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("ahn3-dsm-input-path"))
	{
		std::cerr << "AHN-3 surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn3-dtm-input-path"))
	{
		std::cerr << "AHN-3 terrain input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn2-dsm-input-path"))
	{
		std::cerr << "AHN-2 surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn2-dtm-input-path"))
	{
		std::cerr << "AHN-2 terrain input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!fs::exists(DSMinputPath))
	{
		std::cerr << "The surface input file does not exist." << std::endl;
		argumentError = true;
	}
	if (!fs::exists(DTMinputPath))
	{
		std::cerr << "The terrain input file does not exist." << std::endl;
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

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	Reporter* reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	if (!vm.count("quiet"))
		std::cout << "=== AHN Vegetation Filter ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

    // Configure the operation
    GDALAllRegister();
    std::string lastStatus;
	RasterMetadata targetMetadata, targetMetadata_dummy;
	Process process(3, AHN2DTMinputPath, AHN2DSMinputPath, DTMinputPath, DSMinputPath, outputDir, reporter, vm);


    if (!vm.count("quiet"))
    {
        process.progress = [&reporter, &lastStatus](float complete, const std::string &message)
        {
            if(message != lastStatus)
            {
                std::cout << std::endl
                    << "Task: " << message << std::endl;
                reporter->reset();
                lastStatus = message;
            }
            reporter->report(complete, message);
            return true;
        };
    }

    // Execute operation
	process.execute();
	delete reporter;

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
