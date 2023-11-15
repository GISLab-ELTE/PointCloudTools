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
using namespace CloudTools::Vegetation;

int main(int argc, char* argv[])
{
	std::string dtmInputPathA;
	std::string dsmInputPathA;
	std::string dtmInputPathB;
	std::string dsmInputPathB;
	std::string outputDir = fs::current_path().string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("dsm-input-path-A,x", po::value<std::string>(&dsmInputPathA), "Epoch-A DSM input path")
		("dtm-input-path-A,y", po::value<std::string>(&dtmInputPathA), "Epoch-A DTM input path")
		("dsm-input-path-B,s", po::value<std::string>(&dsmInputPathB), "Epoch-B DSM input path")
		("dtm-input-path-B,t", po::value<std::string>(&dtmInputPathB), "Epoch-B DTM input path")
		("output-dir,o", po::value<std::string>(&outputDir)->default_value(outputDir), "result directory path")
		("hausdorff-distance", "use Hausdorff-distance")
		("srm", "removes trees possibly to close to buildings")
		("parallel,p", "parallel execution for A & B epochs")
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
		std::cout << "Compares DEMs of same area from different epochs and filters out changes in vegetation." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("dsm-input-path-B"))
	{
		std::cerr << "Epoch-B surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("dtm-input-path-B"))
	{
		std::cerr << "Epoch-B terrain input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("dsm-input-path-A"))
	{
		std::cerr << "Epoch-A surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("dtm-input-path-A"))
	{
		std::cerr << "Epoch-A terrain input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!fs::exists(dsmInputPathB))
	{
		std::cerr << "The surface input file does not exist." << std::endl;
		argumentError = true;
	}
	if (!fs::exists(dtmInputPathB))
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
		std::cout << "=== DEM Vegetation Filter ===" << std::endl;
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
	PreProcess preProcessA("a", dtmInputPathA, dsmInputPathA, outputDir,
						   vm.count("srm")
						   ? PreProcess::ProcessingMethod::SeedRemoval
						   : PreProcess::ProcessingMethod::Standard);
	PreProcess preProcessB("b", dtmInputPathB, dsmInputPathB, outputDir,
						   vm.count("srm")
						   ? PreProcess::ProcessingMethod::SeedRemoval
						   : PreProcess::ProcessingMethod::Standard);

	preProcessA.debug = vm.count("debug");
	preProcessB.debug = vm.count("debug");

	if (!vm.count("quiet"))
	{
		if (!vm.count("parallel"))
			preProcessA.progress = preProcessB.progress = progress;
		else
			std::cout << "No progress display for preprocessors in parallel mode." << std::endl;
	}

	// Execute preprocess operations
	auto futureA = std::async(
		vm.count("parallel") ? std::launch::async : std::launch::deferred, &PreProcess::execute, &preProcessA, false);

	auto futureB = std::async(
		vm.count("parallel") ? std::launch::async : std::launch::deferred, &PreProcess::execute, &preProcessB, false);

	futureA.wait();
	futureB.wait();

	// Create the postprocessor
	PostProcess postProcess(
		dsmInputPathA, dsmInputPathB,
		preProcessA.target(), preProcessB.target(),
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
