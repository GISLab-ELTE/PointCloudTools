#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>
#include <chrono>
#include <future>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>

#include "PreProcess.h"
#include "PostProcess.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

int main(int argc, char* argv[])
{
	std::string AHN3DTMinputPath;
	std::string AHN3DSMinputPath;
	std::string AHN2DSMinputPath;
	std::string AHN2DTMinputPath;
	std::string outputDir = fs::current_path().string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn3-dtm-input-path,t", po::value<std::string>(&AHN3DTMinputPath), "AHN3 DTM input path")
		("ahn3-dsm-input-path,s", po::value<std::string>(&AHN3DSMinputPath), "AHN3 DSM input path")
		("ahn2-dtm-input-path,y", po::value<std::string>(&AHN2DTMinputPath), "AHN2 DTM input path")
		("ahn2-dsm-input-path,x", po::value<std::string>(&AHN2DSMinputPath), "AHN2 DSM input path")
		("output-dir,o", po::value<std::string>(&outputDir)->default_value(outputDir), "result directory path")
		("hausdorff-distance", "use Hausdorff-distance")
		("parallel,p", "parallel execution for AHN-2 and AHN-3") // TODO: this will mess up the log output
		("debug,d", "keep intermediate results on disk after progress")
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

	if (!fs::exists(AHN3DSMinputPath))
	{
		std::cerr << "The surface input file does not exist." << std::endl;
		argumentError = true;
	}
	if (!fs::exists(AHN3DTMinputPath))
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
	auto progress = [&reporter, &lastStatus](float complete, const std::string& message)
	{
		if (message != lastStatus)
		{
			std::cout << std::endl
			          << "Task: " << message << std::endl;
			reporter->reset();
			lastStatus = message;
		}
		reporter->report(complete, message);
		return true;
	};

	// Create preprocessors
	PreProcess ahn2PreProcess("ahn2", AHN2DTMinputPath, AHN2DSMinputPath, outputDir);
	PreProcess ahn3PreProcess("ahn3", AHN3DTMinputPath, AHN3DSMinputPath, outputDir);

	ahn2PreProcess.debug = vm.count("debug");
	ahn3PreProcess.debug = vm.count("debug");

	if (!vm.count("quiet"))
	{
		if (!vm.count("parallel"))
			ahn2PreProcess.progress = ahn3PreProcess.progress = progress;
		else
			std::cout << "No progress display for preprocessors in parallel mode." << std::endl;
	}

	// Execute preprocess operations
	auto ahn2Future = std::async(
		vm.count("parallel") ? std::launch::async : std::launch::deferred, &PreProcess::execute, &ahn2PreProcess, false);

	auto ahn3Future = std::async(
		vm.count("parallel") ? std::launch::async : std::launch::deferred, &PreProcess::execute, &ahn3PreProcess, false);

	ahn2Future.wait();
	ahn3Future.wait();

	// Create the postprocessor
	PostProcess postProcess(
		AHN2DSMinputPath, AHN3DSMinputPath,
		ahn2PreProcess.target(), ahn3PreProcess.target(),
		outputDir,
		vm.count("hausdorff-distance")
		? PostProcess::DifferenceMethod::Hausdorff
		: PostProcess::DifferenceMethod::Centroid);

	if (!vm.count("quiet"))
	{
		postProcess.progress = progress;
	}

	// Execute postprocess operations
	postProcess.execute();
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
