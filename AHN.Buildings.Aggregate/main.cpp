#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal_priv.h>
#include <gdal_utils.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include <CloudTools.DEM/Metadata.h>
#include <CloudTools.DEM/Rasterize.h>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include "Region.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::DEM;
using namespace CloudTools::IO;
using namespace AHN;

const char* LabelGained = "ALT_GAINED";
const char* LabelLost = "ALT_LOST";
const char* LabelMoved = "ALT_MOVED";
const char* LabelDifference = "ALT_DIFF";
const char* ResultFile = "/vsimem/out.shp";

int main(int argc, char* argv[]) try
{
	std::string ahnDir;
	std::string adminVectorFile;
	std::string adminRasterDir = fs::current_path().string();
	std::string outputFile = (fs::current_path() / "out.shp").string();
	std::string webFile = (fs::current_path() / "out.json").string();
	std::string adminLayer;
	std::string adminField;

	bool webEnable = false;
	float webTolerance = 5.f;
	std::string webSRS = "EPSG:900913";

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn-dir", po::value<std::string>(&ahnDir), "directory path for AHN changeset tiles")
		("admin-vector", po::value<std::string>(&adminVectorFile), "file path for vector administrative unit file")
		("admin-layer", po::value<std::string>(&adminLayer), "layer name for administrative units")
		("admin-field", po::value<std::string>(&adminField), "attribute field name for unit identifier")
		("admin-raster", po::value<std::string>(&adminRasterDir), "directory path for raster administrative unit tiles")
		("output-file", po::value<std::string>(&outputFile)->default_value(outputFile), "output vector file path (Shapefile)")
		("force,f", "regenerates the raster tiles even when they exist")
		("web-output,w", "generates GeoJSON output for web support")
		("web-tolerance", po::value<float>(&webTolerance)->default_value(webTolerance),
			"tolerance for web output polygon generalization")
		("web-srs", po::value<std::string>(&webSRS)->default_value(webSRS),
			"spatial reference system for web output (reprojection)")		
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Post-processing arguments
	if (vm.count("output-file"))
	{
		outputFile = fs::path(outputFile).replace_extension(".shp").string();
		webFile = fs::path(outputFile).replace_extension(".json").string();
	}
	if (vm.count("web-output") || !vm["web-tolerance"].defaulted() || !vm["web-srs"].defaulted())
		webEnable = true;

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Computes aggregative change of volume for administrative units." << std::endl;
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

	if (!vm.count("admin-field"))
	{
		std::cerr << "The attribute field name for administrative unit identifier is mandatory." << std::endl;
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

	// Calculating aggregated altimetry change data
	std::map<int, Region> results;
	for (fs::directory_iterator ahnFile(ahnDir); ahnFile != fs::directory_iterator(); ++ahnFile)
	{
		if (fs::is_regular_file(ahnFile->status()) && ahnFile->path().extension() == ".tif")
		{
			fs::path ahnPath = ahnFile->path();
			fs::path adminRasterPath = adminRasterDir / ahnPath.filename();
			std::cout << std::endl
				      << "Processing tile: " << ahnPath.stem() << std::endl;
			reporter.reset();

			if (!fs::exists(adminRasterPath) || vm.count("force"))
			{
				// Create the admin raster tile for the AHN tile
				Rasterize rasterizer(adminVectorFile, adminRasterPath.string(), 
					!adminLayer.empty() ? std::vector<std::string> { adminLayer } : std::vector<std::string>());
				rasterizer.targetField = adminField;
				rasterizer.targetType = GDALDataType::GDT_Int32;
				rasterizer.createOptions.insert(std::make_pair("COMPRESS", "DEFLATE"));
				rasterizer.progress = [&reporter](float complete, const std::string &message)
				{
					reporter.report(complete / 2, message);
					return true;
				};

				// Define clipping for raster filter
				GDALDataset* ahnDataset = static_cast<GDALDataset*>(GDALOpen(ahnPath.string().c_str(), GA_ReadOnly));
				if (ahnDataset == nullptr)
					throw std::runtime_error("Error at opening the AHN tile.");

				RasterMetadata ahnMetadata(ahnDataset);
				rasterizer.pixelSizeX = ahnMetadata.pixelSizeX();
				rasterizer.pixelSizeY = ahnMetadata.pixelSizeY();
				rasterizer.clip(ahnMetadata.originX(), ahnMetadata.originY(),
				                ahnMetadata.rasterSizeX(), ahnMetadata.rasterSizeY());
				GDALClose(ahnDataset);

				// Execute operation
				rasterizer.execute();
			}
			else // Raster tile already exists
				reporter.report(.5f, std::string());

			// Altimetry change aggregation
			SweepLineCalculation<double> calculation({ ahnPath.string(), adminRasterPath.string() },
				[&results](int x, int y, const std::vector<Window<double>>& data) // NOT float
			{
				const auto& ahn = data[0];
				const auto& admin = data[1];

				if (admin.hasData())
				{
					int id = static_cast<int>(admin.data());
					if (results.find(id) == results.end())
						results[id].id = id;

					if (!ahn.hasData()) return;
					float change = static_cast<float>(ahn.data());

					if (change > 0)
						results[id].gained += change;
					if (change < 0)
						results[id].lost -= change;
					results[id].moved += std::abs(change);
					results[id].difference += change;
				}
			}, 
				[&reporter](float complete, const std::string &message)
			{
				reporter.report(.5f + complete / 2, message);
				return true;
			});
			calculation.spatialReference = "EPSG:28992";
			
			// Execute operation
			calculation.execute();
		}
	}

	// Creating output Shapefile 
	std::cout << std::endl << "Generating output ... ";
	GDALDataset *adminDataset = static_cast<GDALDataset*>(GDALOpenEx(adminVectorFile.c_str(),
	                                                                 GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
	if (adminDataset == nullptr)
		throw std::runtime_error("Error at opening the admin vector file.");

	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	GDALDataset *resultDataset = driver->CreateCopy(ResultFile, adminDataset,
	                                                false, nullptr, nullptr, nullptr);
	GDALClose(adminDataset);
	if (resultDataset == nullptr)
		throw std::runtime_error("Error at creating result dataset.");

	// Selecting layer
	OGRLayer *layer;
	if (vm.count("admin-layer"))
	{
		layer = resultDataset->GetLayerByName(adminLayer.c_str());
		if (layer == nullptr)
			throw std::invalid_argument("The selected layer does not exist.");
	}
	else if (resultDataset->GetLayerCount() == 1)
		layer = resultDataset->GetLayer(0);
	else
		throw std::invalid_argument("No layer selected and there are more than 1 layers.");

	// Checking admin attribute field and removing conflicting result attribute fields
	{
		int index;
		if ((index = layer->FindFieldIndex(adminField.c_str(), true)) < 0)
			throw std::invalid_argument("The attribute field name for administrative unit identifier was not found.");
		if ((index = layer->FindFieldIndex(LabelGained, true)) >= 0)
			layer->DeleteField(index);
		if ((index = layer->FindFieldIndex(LabelLost, true)) >= 0)
			layer->DeleteField(index);
		if ((index = layer->FindFieldIndex(LabelMoved, true)) >= 0)
			layer->DeleteField(index);
		if ((index = layer->FindFieldIndex(LabelDifference, true)) >= 0)
			layer->DeleteField(index);
	}

	// Adding result attribute fields
	OGRFieldDefn fieldGained(LabelGained, OGRFieldType::OFTInteger);
	OGRFieldDefn fieldLost(LabelLost, OGRFieldType::OFTInteger);
	OGRFieldDefn fieldMoved(LabelMoved, OGRFieldType::OFTInteger);
	OGRFieldDefn fieldDifference(LabelDifference, OGRFieldType::OFTInteger);
	layer->CreateField(&fieldGained);
	layer->CreateField(&fieldLost);
	layer->CreateField(&fieldMoved);
	layer->CreateField(&fieldDifference);

	// Writing attribute data
	OGRFeature *feature;
	OGRErr featureError = OGRERR_NONE;
	std::set<GIntBig> emptyFeatures;
	layer->ResetReading();
	while ((feature = layer->GetNextFeature()) != nullptr)
	{
		int id = feature->GetFieldAsInteger(adminField.c_str());
		auto result = results.find(id);
		if (result != results.end())
		{
			feature->SetField(LabelGained, std::round(result->second.gained));
			feature->SetField(LabelLost, std::round(result->second.lost));
			feature->SetField(LabelMoved, std::round(result->second.moved));
			feature->SetField(LabelDifference, std::round(result->second.difference));
			featureError = static_cast<OGRErr>(featureError |
				layer->SetFeature(feature));
		}
		else
			emptyFeatures.insert(feature->GetFID());
		OGRFeature::DestroyFeature(feature);
	}
	if(featureError != OGRERR_NONE)
		throw std::logic_error("Could not set the arregated fields for an admin unit.");

	// Removing features out of scope
	for (auto id : emptyFeatures)
		featureError = static_cast<OGRErr>(featureError | 
			layer->DeleteFeature(id));
	if (featureError != OGRERR_NONE)
		throw std::logic_error("Could not remove an unrequired admin unit.");
	emptyFeatures.clear();
	std::cout << "done." << std::endl;

	// Writing Shapefile output
	std::cout << "Writing output (Shapefile format) ... ";
	if (fs::exists(outputFile) &&
		driver->Delete(outputFile.c_str()) == CE_Failure &&
		!fs::remove(outputFile))
		throw std::runtime_error("Cannot overwrite previously created output file.");

	GDALDataset *outputDataset = driver->CreateCopy(outputFile.c_str(), resultDataset,
	                                                false, nullptr, nullptr, nullptr);
	if (outputDataset == nullptr)
	{
		GDALClose(resultDataset);
		throw std::runtime_error("Error at creating the output file.");
	}
	GDALClose(outputDataset);
	std::cout << "done." << std::endl;

	// Writing GeoJSON output
	if (webEnable)
	{
		std::cout << "Writing output (GeoJSON format) ... ";
		if (fs::exists(webFile) &&
			!fs::remove(webFile))
			throw std::runtime_error("Cannot overwrite previously created JSON output file.");

		// Define the GDALVectorTranslate parameters
		char **params = nullptr;
		params = CSLAddString(params, "-f");
		params = CSLAddString(params, "GeoJSON");
		params = CSLAddString(params, "-t_srs");
		params = CSLAddString(params, webSRS.c_str());
		if (webTolerance > 0)
		{
			params = CSLAddString(params, "-simplify");
			params = CSLAddString(params, std::to_string(webTolerance).c_str());
		}

		// Execute GDALVectorTranslate
		GDALVectorTranslateOptions *options = GDALVectorTranslateOptionsNew(params, nullptr);
		GDALDatasetH sources[1] = {resultDataset};
		GDALDataset* webDataset = static_cast<GDALDataset*>(GDALVectorTranslate(webFile.c_str(), nullptr,
		                                                                        1, sources,
		                                                                        options, nullptr));
		GDALVectorTranslateOptionsFree(options);
		CSLDestroy(params);

		if (webDataset == nullptr)
		{
			GDALClose(resultDataset);
			throw std::runtime_error("Error at creating the JSON output file.");
		}
		GDALClose(webDataset);
		std::cout << "done." << std::endl;
	}

	GDALClose(resultDataset);
	VSIUnlink(ResultFile);

	// Execution time measurement
	std::clock_t clockEnd = std::clock();
	auto timeEnd = std::chrono::high_resolution_clock::now();

	std::cout << std::endl
		<< "All completed!" << std::endl << std::fixed << std::setprecision(2)
		<< "CPU time used: "
		<< 1.f * (clockEnd - clockStart) / CLOCKS_PER_SEC / 60 << " min" << std::endl
		<< "Wall clock time passed: "
		<< std::chrono::duration<float>(timeEnd - timeStart).count() / 60 << " min" << std::endl;

	return Success;
}
catch (std::exception &ex)
{
	std::cerr << "ERROR: " << ex.what() << std::endl;
	return UnexcpectedError;
}
