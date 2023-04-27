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

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Metadata.h>
#include <CloudTools.DEM/Rasterize.h>
#include <CloudTools.DEM/SweepLineTransformation.hpp>
#include <CloudTools.DEM/Window.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::DEM;
using namespace CloudTools::IO;

int main(int argc, char* argv[]) try
{
	std::string inputPath;
	std::string outputPath = (fs::current_path() / "out.tif").string();
	std::string outputFormat;
	std::vector<std::string> outputOptions;

	std::string maskVectorPath;
	std::vector<std::string> maskLayers;
	std::string maskRasterPath = (fs::current_path() / fs::unique_path()).replace_extension("tif").string();
	short maskValue;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("input-path,i", po::value<std::string>(&inputPath), "input path")
		("mask-vector,f", po::value<std::string>(&maskVectorPath), "vector mask path")
		("mask-layer,l", po::value<std::vector<std::string>>(&maskLayers), "mask layer(s) name")
		("mask-raster,r", po::value<std::string>(&maskRasterPath),
			"raster mask path (only GTiff supported);\n"
			"when does not exist, will be generated from mask-vector")
		("mask-value", po::value<short>(&maskValue)->default_value(255),
			"specifies the value for the raster mask generation")
		("output-path,o", po::value<std::string>(&outputPath), "output path")
		("output-format", po::value<std::string>(&outputFormat)->default_value("GTiff"),
			"output format, supported formats:\n"
			"http://www.gdal.org/formats_list.html")
		("output-option", po::value<std::vector<std::string>>(&outputOptions),
			"output options, supported options:\n"
			"http://www.gdal.org/formats_list.html")
		("nodata-value", po::value<double>(), "specifies the output nodata value for masked out values")
		("srs", po::value<std::string>(), "override spatial reference system")
		("invert", "invert the mask input")
		("force", "regenerates the raster mask even if exists")
		("verbose,v", "verbose output")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Post-processing arguments
	if (vm.count("mask-raster"))
		maskRasterPath = fs::path(maskRasterPath).replace_extension("tif").string();

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Transforms a vector mask into a raster mask and/or applies the latter on a DEM." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("mask-vector") && !vm.count("mask-raster"))
	{
		std::cerr << "Either vector or raster mask must be given." << std::endl;
		argumentError = true;
	}

	if (!vm.count("input-path"))
	{
		std::cerr << "Input file must be given." << std::endl;
		argumentError = true;
	}

	if (vm.count("force") && !vm.count("mask-vector"))
	{
		std::cerr << "The force option can only be used when vector mask is given." << std::endl;
		argumentError = true;
	}

	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	if (vm.count("verbose"))
		std::cout << "=== DEM Mask Tool ===" << std::endl;

	Reporter *reporter = vm.count("verbose")
		? static_cast<Reporter*>(new TextReporter())
		: static_cast<Reporter*>(new BarReporter());

	GDALAllRegister();
	if (!fs::exists(maskRasterPath) || vm.count("force"))
	{
		// Create the raster mask
		Rasterize rasterizer(maskVectorPath, maskRasterPath, maskLayers);
		rasterizer.targetValue = static_cast<uint8_t>(maskValue);
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

		// Define clipping for raster mask
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
			std::cerr << "Skipping raster mask generation."
				<< " Skipping output file generation."
				<< std::endl;
			return NoResult;
		}

		// Display mask output metadata
		if (vm.count("verbose"))
		{
			std::cout << std::endl << "--- Vector mask ---" << std::endl;
			std::cout << "File path: \t" << maskVectorPath << std::endl
				<< rasterizer.sourceMetadata() << std::endl;

			std::cout << std::endl << "--- Raster mask ---" << std::endl;
			std::cout << "File path: \t" << maskRasterPath << std::endl
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
		std::cout << "Skipping raster mask generation, already exists." << std::endl;


	// Read input data type
	GDALDataset* inputDataset = static_cast<GDALDataset*>(GDALOpen(inputPath.c_str(), GA_ReadOnly));
	if (inputDataset == nullptr)
		throw std::runtime_error("Error at opening the input file.");

	GDALRasterBand* inputBand = inputDataset->GetRasterBand(1);
	GDALDataType dataType = inputBand->GetRasterDataType();
	GDALClose(inputDataset);

	// Define mask with corresponding data type
	Transformation *mask;
	switch (dataType)
	{
	case GDALDataType::GDT_Int16:
	{
		mask = new SweepLineTransformation<GInt16>({ inputPath, maskRasterPath }, outputPath,
			[&vm, &mask](int x, int y, const std::vector<Window<GInt16>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: static_cast<GInt16>(mask->nodataValue);
		});
		break;
	}
	case GDALDataType::GDT_Int32:
	{
		mask = new SweepLineTransformation<GInt32>({ inputPath, maskRasterPath }, outputPath,
			[&vm, &mask](int x, int y, const std::vector<Window<GInt32>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: static_cast<GInt32>(mask->nodataValue);
		});
		break;
	}
	case GDALDataType::GDT_Float32:
	{
		mask = new SweepLineTransformation<float>({ inputPath, maskRasterPath }, outputPath,
			[&vm, &mask](int x, int y, const std::vector<Window<float>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: static_cast<float>(mask->nodataValue);
		});
		break;
	}
	case GDALDataType::GDT_Float64:
	{
		mask = new SweepLineTransformation<double>({ inputPath, maskRasterPath }, outputPath,
			[&vm, &mask](int x, int y, const std::vector<Window<double>>& sources)
		{
			return (!vm.count("invert") ? sources[1].hasData() : !sources[1].hasData()) && sources[0].hasData()
				? sources[0].data()
				: mask->nodataValue;
		});
		break;
	}
	default:
		// Unsigned and complex types are not supported.
		std::cerr << "Unsupported data type given." << std::endl;
		return Unsupported;
	}

	if (vm.count("nodata-value"))
		mask->nodataValue = vm["nodata-value"].as<double>();
	if (vm.count("srs"))
		mask->spatialReference = vm["srs"].as<std::string>();
	if (!vm.count("quiet"))
	{
		mask->progress = [&reporter](float complete, const std::string &message)
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
			mask->createOptions.insert(std::make_pair(key, value));
		}
	}

	// Prepare operation
	mask->prepare();

	// Display input and output metadata
	if (vm.count("verbose"))
	{
		std::cout << std::endl << "--- Input file ---" << std::endl;
		const RasterMetadata &inputMetadata = mask->sourceMetadata(inputPath);
		std::cout << "File path: \t" << inputPath << std::endl
			<< inputMetadata << std::endl;

		std::cout << std::endl << "--- Mask file ---" << std::endl;
		const RasterMetadata &maskMetadata = mask->sourceMetadata(maskRasterPath);
		std::cout << "File path: \t" << maskRasterPath << std::endl
			<< maskMetadata << std::endl;

		std::cout << std::endl << "--- Output file ---" << std::endl;
		const RasterMetadata& outputMetadata = mask->targetMetadata();
		std::cout << "File path: \t" << outputPath << std::endl
			<< outputMetadata << std::endl;

		if (!readBoolean("Would you like to continue?"))
		{
			std::cerr << "Operation aborted." << std::endl;
			return UserAbort;
		}
	}

	// Execute operation
	mask->execute();
	delete mask;
	delete reporter;

	// Remove temporary mask raster file
	if (!vm.count("mask-raster"))
	{
		GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
		if (fs::exists(maskRasterPath) &&
			driver->Delete(maskRasterPath.c_str()) == CE_Failure)
			std::cerr << "Cannot remove temporary raster mask file." << std::endl;
	}
	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
