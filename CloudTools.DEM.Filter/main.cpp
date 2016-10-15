#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <cstdint>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include <CloudLib.DEM/Metadata.h>
#include <CloudLib.DEM/Rasterize.h>
#include <CloudLib.DEM/SweepLineTransformation.h>
#include <CloudLib.DEM/Window.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudLib::DEM;
using namespace CloudTools::IO;

int main(int argc, char* argv[]) try
{
	std::string inputPath;
	std::string outputPath = (fs::current_path() / "out.tif").string();
	std::string outputFormat;
	std::vector<std::string> outputOptions;

	std::string filterVectorPath;
	std::vector<std::string> filterLayers;
	std::string filterRasterPath = (fs::current_path() / std::tmpnam(nullptr)).replace_extension("tif").string();
	short filterValue;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("input-path,i", po::value<std::string>(&inputPath), "input path")
		("filter-vector,f", po::value<std::string>(&filterVectorPath), "vector filter path")
		("filter-layer,l", po::value<std::vector<std::string>>(&filterLayers), "filter layer(s) name")
		("filter-raster,r", po::value<std::string>(&filterRasterPath),
			"raster filter path (only GTiff supported);\n"
			"when does not exist, will be generated from filter-vector")
		("filter-value", po::value<short>(&filterValue)->default_value(255),
			"specifies the value for the raster filter generation")
		("output-path,o", po::value<std::string>(&outputPath), "output path")
		("output-format", po::value<std::string>(&outputFormat)->default_value("GTiff"),
			"output format, supported formats:\n"
			"http://www.gdal.org/formats_list.html")
		("output-option", po::value<std::vector<std::string>>(&outputOptions),
			"output options, supported options:\n"
			"http://www.gdal.org/formats_list.html")
		("nodata-value", po::value<double>(), "specifies the output nodata value for filtered out values")
		("srs", po::value<std::string>(), "override spatial reference system")
		("invert", "invert the filter input")
		("force", "regenerates the raster filter even if exists")
		("verbose,v", "verbose output")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Post-processing arguments
	if (vm.count("filter-raster"))
		filterRasterPath = fs::path(filterRasterPath).replace_extension("tif").string();

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Transforms a vector filter into a raster filter and/or applies the latter on a DEM." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("filter-vector") && !vm.count("filter-raster"))
	{
		std::cerr << "Either vector or raster filter must be given." << std::endl;
		argumentError = true;
	}

	if (!vm.count("input-path"))
	{
		std::cerr << "Input file must be given." << std::endl;
		argumentError = true;
	}

	if (vm.count("force") && !vm.count("filter-vector"))
	{
		std::cerr << "The force option can only be used when vector filter is given." << std::endl;
		argumentError = true;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	if (vm.count("verbose"))
		std::cout << "=== DEM Filter Tool ===" << std::endl;

	Reporter *reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	GDALAllRegister();
	if (!fs::exists(filterRasterPath) || vm.count("force"))
	{
		// Create the raster filter
		Rasterize rasterizer(filterVectorPath, filterRasterPath, filterLayers);
		rasterizer.targetValue = static_cast<uint8_t>(filterValue);
		rasterizer.createOptions.insert(std::make_pair("COMPRESS", "DEFLATE"));
		if (vm.count("srs"))
			rasterizer.spatialReference = vm["srs"].as<std::string>();
		if (!vm.count("quiet"))
		{
			rasterizer.progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}

		// Define clipping for raster filter
		GDALDataset* inputDataset = static_cast<GDALDataset*>(GDALOpen(inputPath.c_str(), GA_ReadOnly));
		if (inputDataset == nullptr)
			throw std::runtime_error("Error at opening the input file.");

		RasterMetadata inputMetadata(inputDataset);
		rasterizer.pixelSizeX = inputMetadata.pixelSizeX();
		rasterizer.pixelSizeY = inputMetadata.pixelSizeY();
		rasterizer.clip(inputMetadata.originX(), inputMetadata.originY(),
			inputMetadata.rasterSizeX(), inputMetadata.rasterSizeY());
		GDALClose(inputDataset);

		// Prepare operation
		try
		{
			rasterizer.prepare();
		}
		catch (std::logic_error &ex)
		{
			std::cerr << "WARNING: " << ex.what() << std::endl;
			std::cerr << "Skipping raster filter generation."
				<< " Skipping output file generation."
				<< std::endl;
			return NoResult;
		}

		// Display filter output metadata
		if (vm.count("verbose"))
		{
			std::cout << std::endl << "--- Vector filter ---" << std::endl;
			std::cout << "File path: \t" << filterVectorPath << std::endl
				<< rasterizer.sourceMetadata() << std::endl;

			std::cout << std::endl << "--- Raster filter ---" << std::endl;
			std::cout << "File path: \t" << filterRasterPath << std::endl
				<< rasterizer.targetMetadata() << std::endl;

			if (!readBoolean("Would you like to continue?"))
			{
				std::cerr << "Operation aborted." << std::endl;
				return UserAbort;
			}
		}

		// Execute operation
		rasterizer.execute();
		reporter->reset();
	}
	else if (vm.count("verbose"))
		std::cout << "Skipping raster filter generation, already exists." << std::endl;


	// Read input data type
	GDALDataset* inputDataset = static_cast<GDALDataset*>(GDALOpen(inputPath.c_str(), GA_ReadOnly));
	if (inputDataset == nullptr)
		throw std::runtime_error("Error at opening the input file.");

	GDALRasterBand* inputBand = inputDataset->GetRasterBand(1);
	GDALDataType dataType = inputBand->GetRasterDataType();
	GDALClose(inputDataset);

	// Define filter with corresponding data type
	Transformation *filter;
	switch (dataType)
	{
	case GDALDataType::GDT_Int16:
	{
		filter = new SweepLineTransformation<GInt16>({ inputPath, filterRasterPath }, outputPath,
			[&vm, &filter](int x, int y, const std::vector<Window<GInt16>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: static_cast<GInt16>(filter->nodataValue);
		});
		break;
	}
	case GDALDataType::GDT_Int32:
	{
		filter = new SweepLineTransformation<GInt32>({ inputPath, filterRasterPath }, outputPath,
			[&vm, &filter](int x, int y, const std::vector<Window<GInt32>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: static_cast<GInt32>(filter->nodataValue);
		});
		break;
	}
	case GDALDataType::GDT_Float32:
	{
		filter = new SweepLineTransformation<float>({ inputPath, filterRasterPath }, outputPath,
			[&vm, &filter](int x, int y, const std::vector<Window<float>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: static_cast<float>(filter->nodataValue);
		});
		break;
	}
	case GDALDataType::GDT_Float64:
	{
		filter = new SweepLineTransformation<double>({ inputPath, filterRasterPath }, outputPath,
			[&vm, &filter](int x, int y, const std::vector<Window<double>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: filter->nodataValue;
		});
		break;
	}
	default:
		// Unsigned and complex types are not supported.
		std::cerr << "Unsupported data type given." << std::endl;
		return Unsupported;
	}

	if (vm.count("nodata-value"))
		filter->nodataValue = vm["nodata-value"].as<double>();
	if (vm.count("srs"))
		filter->spatialReference = vm["srs"].as<std::string>();
	if (!vm.count("quiet"))
	{
		filter->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	if (vm.count("output-option"))
	{
		for (const std::string &option : outputOptions)
		{
			auto pos = std::find(option.begin(), option.end(), '=');
			if (pos == option.end()) continue;
			std::string key = option.substr(0, pos - option.begin());
			std::string value = option.substr(pos - option.begin() + 1, option.end() - pos - 1);
			filter->createOptions.insert(std::make_pair(key, value));
		}
	}

	// Prepare operation
	filter->prepare();

	// Display input and output metadata
	if (vm.count("verbose"))
	{
		std::cout << std::endl << "--- Input file ---" << std::endl;
		const RasterMetadata &inputMetadata = filter->sourceMetadata(inputPath);
		std::cout << "File path: \t" << inputPath << std::endl
			<< inputMetadata << std::endl;

		std::cout << std::endl << "--- Filter file ---" << std::endl;
		const RasterMetadata &filterMetadata = filter->sourceMetadata(filterRasterPath);
		std::cout << "File path: \t" << filterRasterPath << std::endl
			<< filterMetadata << std::endl;

		std::cout << std::endl << "--- Output file ---" << std::endl;
		const RasterMetadata& outputMetadata = filter->targetMetadata();
		std::cout << "File path: \t" << outputPath << std::endl
			<< outputMetadata << std::endl;

		if (!readBoolean("Would you like to continue?"))
		{
			std::cerr << "Operation aborted." << std::endl;
			return UserAbort;
		}
	}

	// Execute operation
	filter->execute();
	delete filter;

	// Remove temporary filter raster file
	if (!vm.count("filter-raster"))
	{
		GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
		if (fs::exists(filterRasterPath) &&
			driver->Delete(filterRasterPath.c_str()) == CE_Failure)
			std::cerr << "Cannot remove temporary raster filter file." << std::endl;
	}
	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
