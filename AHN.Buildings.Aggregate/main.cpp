#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <utility>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include <CloudLib.DEM/Metadata.h>
#include <CloudLib.DEM/Rasterize.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudLib::DEM;
using namespace CloudTools::IO;

int main(int argc, char* argv[]) try
{
	fs::path ahnDir;
	fs::path adminVectorFile;
	fs::path adminRasterDir = fs::current_path();
	fs::path outputDir = fs::current_path();
	std::string adminLayer;
	std::string adminField;	

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn-dir", po::value<fs::path>(&ahnDir), "directory path for AHN tiles")
		("admin-vector", po::value<fs::path>(&adminVectorFile), "file path for vector administrative unit file")
		("admin-layer", po::value<std::string>(&adminLayer), "layer name for administrative units")
		("admin-field", po::value<std::string>(&adminField), "attribute field name for unit identifier")
		("admin-raster", po::value<fs::path>(&adminRasterDir), "directory path for raster administrative unit tiles")
		("output-file", po::value<fs::path>(&outputDir), "output vector file path")
		("nodata-value", po::value<double>(), "specifies the output nodata value for filtered out values")
		("force", "regenerates the raster tiles even when they exist")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Transforms a vector filter into a raster filter and/or applies the latter on a DEM." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("ahn-dir"))
	{
		std::cerr << "Input directory is mandatory." << std::endl;
		argumentError = true;
	}
	if (!fs::is_directory(ahnDir))
	{
		std::cerr << "The input directory does not exist." << std::endl;
		argumentError = true;
	}

	if (!vm.count("admin-vector"))
	{
		std::cerr << "The administrative unit vector file is mandatory." << std::endl;
		argumentError = true;
	}
	if (!fs::is_regular_file(adminVectorFile))
	{
		std::cerr << "The administrative unit vector file does not exist." << std::endl;
		argumentError = true;
	}

	if (fs::exists(adminRasterDir) && !fs::is_directory(adminRasterDir))
	{
		std::cerr << "The given administrative unit raster tile path exists but not a directory." << std::endl;
		argumentError = true;
	}
	else if (!fs::exists(adminRasterDir) && !fs::create_directory(adminRasterDir))
	{
		std::cerr << "Failed to create administrative unit raster tile directory." << std::endl;
		argumentError = true;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	std::cout << "=== AHN Building Filter Aggregator ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

	BarReporter reporter;
	GDALAllRegister();

	for (fs::directory_iterator ahnFile(ahnDir); ahnFile != fs::directory_iterator(); ++ahnFile)
	{
		if (fs::is_regular_file(ahnFile->status()) && ahnFile->path().extension() == ".tif")
		{
			fs::path ahnPath = ahnFile->path();
			fs::path adminPath = adminRasterDir / ahnPath.filename();
			std::cout << std::endl
				<< "Processing tile: " << ahnPath.stem() << std::endl;
			reporter.reset();

			if (!fs::exists(adminPath) || vm.count("force"))
			{
				// Create the raster filter
				Rasterize rasterizer(adminVectorFile.string(), adminPath.string(), 
					!adminLayer.empty() ? std::vector<std::string> { adminLayer } : std::vector<std::string>());
				rasterizer.targetField = adminField;
				rasterizer.createOptions.insert(std::make_pair("COMPRESS", "DEFLATE"));
				rasterizer.progress = [&reporter](float complete, std::string message)
				{
					reporter.report(complete / 2, message);
					return true;
				};

				// Define clipping for raster filter
				GDALDataset* tileDataset = static_cast<GDALDataset*>(GDALOpen(ahnPath.string().c_str(), GA_ReadOnly));
				if (tileDataset == nullptr)
					throw std::runtime_error("Error at opening the input file.");

				RasterMetadata tileMetadata(tileDataset);
				rasterizer.pixelSizeX = tileMetadata.pixelSizeX();
				rasterizer.pixelSizeY = tileMetadata.pixelSizeY();
				rasterizer.clip(tileMetadata.originX(), tileMetadata.originY(),
					tileMetadata.rasterSizeX(), tileMetadata.rasterSizeY());
				GDALClose(tileDataset);

				// Prepare operation
				try
				{
					rasterizer.prepare();
				}
				catch (std::logic_error ex)
				{
					continue;
				}

				rasterizer.execute();
			}
			else
				reporter.report(.5f, std::string());

			reporter.report(1.f, std::string());
		}
	}
	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
