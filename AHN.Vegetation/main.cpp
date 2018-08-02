#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.Common/Reporter.h>
#include "NoiseFilter.h"

namespace po = boost::program_options;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

int main(int argc, char* argv[])
{
	std::string DTMinputPath;
	std::string DSMinputPath;
	std::string outputPath = (fs::current_path() / "out.tif").string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("dtm-input-path,t", po::value<std::string>(&DTMinputPath), "DTM input path")
		("dsm-input-path,s", po::value<std::string>(&DSMinputPath), "DSM input path")
		("output-path,o", po::value<std::string>(&outputPath)->default_value(outputPath), "output path")
		("verbose,v", "verbose output")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Compares an AHN-2 and AHN-3 tile pair and filters out changes in vegtation." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("dsm-input-path"))
	{
		std::cerr << "Surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("dtm-input-path"))
	{
		std::cerr << "Terrain input file is mandatory." << std::endl;
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

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	Reporter *reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	if (!vm.count("quiet"))
		std::cout << "=== AHN Vegetation Filter ===" << std::endl;

	GDALAllRegister();

	// Generate CHM
	Difference<float> *comparison = new Difference<float>({ DTMinputPath, DSMinputPath }, "CHM.tif");
	if (!vm.count("quiet"))
	{
		comparison->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}

	comparison->execute();
	delete comparison;
	
	// Vegetation filter
	reporter->reset();
	NoiseFilter* filter = new NoiseFilter("CHM.tif", outputPath, 3);
	if (!vm.count("quiet"))
	{
		filter->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}

	filter->execute();
	delete filter;
	delete reporter;
	return Success;
}