#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO.h>
#include <Cloudtools.DEM/Comparers/Difference.hpp>
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

	if (vm.count("help"))
	{
		std::cout << "Compares DEMs of same area to retrieve differences." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	Reporter *reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	GDALAllRegister();
	/*Transformation *comparison = new Difference<float>({ DTMinputPath, DSMinputPath }, "CHM.tif");

	comparison->progress = [&reporter](float complete, const std::string &message)
	{
		reporter->report(complete, message);
		return true;
	};

	comparison->execute();
	delete comparison;

	reporter->reset();*/

	NoiseFilter* filter = new NoiseFilter("CHM.tif", outputPath, 3);

	filter->progress = [&reporter](float complete, const std::string &message)
	{
		reporter->report(complete, message);
		return true;
	};

	filter->execute();	
	delete filter;

	return Success;
}