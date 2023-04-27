#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <ctime>
#include <stdexcept>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Metadata.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::DEM;
using namespace CloudTools::IO;

struct Tree
{
	OGRPoint location;
	int year;
	int radius;
};

void writeResultsToFile(
	const std::vector<Tree>& matched,
	const std::vector<Tree>& missed,
	VectorMetadata metadata,
	const std::string& outPath);

int main(int argc, char* argv[]) try
{
	std::string inputFile;
	std::string referenceFile;

	std::string yearField = "Plantjaar";
	std::string radiusField = "RADIUS";

	unsigned int minYear = 0;
	unsigned int maxYear = 9999;
	unsigned int minRadius = 0;
	unsigned int minTolerance = 3;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("input,i", po::value<std::string>(&inputFile),
		 "cluster map file to verify")
		("reference,r", po::value<std::string>(&referenceFile),
		 "reference file to verify against")
		("reference-year", po::value<std::string>(&yearField)->default_value(yearField),
		 "year field in reference file")
		("reference-radius", po::value<std::string>(&radiusField)->default_value(radiusField),
		 "radius field in reference file")
		("min-year", po::value<unsigned int>(&minYear)->default_value(minYear),
		 "minimum plant year")
		("max-year", po::value<unsigned int>(&maxYear)->default_value(maxYear),
		 "maximum plant year")
		("min-radius", po::value<unsigned int>(&minRadius)->default_value(minRadius),
		 "minimum tree radius")
		("min-tolerance", po::value<unsigned int>(&minTolerance)->default_value(minTolerance),
		 "minimum distance tolerance for matching")
		("verbose,v", "verbose output")
		("help,h", "produce help message");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Verifies detected trees against reference file." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("input"))
	{
		std::cerr << "The input file is mandatory." << std::endl;
		argumentError = true;
	}
	if (!fs::is_regular_file(inputFile))
	{
		std::cerr << "The input file does not exist." << std::endl;
		argumentError = true;
	}

	if (!vm.count("reference"))
	{
		std::cerr << "The reference file is mandatory." << std::endl;
		argumentError = true;
	}
	if (!fs::is_regular_file(referenceFile))
	{
		std::cerr << "The reference file does not exist." << std::endl;
		argumentError = true;
	}


	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	std::cout << "=== DEM Vegetation Filter Verifier ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

	Reporter* reporter = vm.count("verbose")
	                     ? static_cast<Reporter*>(new TextReporter())
	                     : static_cast<Reporter*>(new BarReporter());

	GDALAllRegister();

	// Open datasets
	GDALDataset* inputDataset = static_cast<GDALDataset*>(GDALOpenEx(inputFile.c_str(),
	                                                                 GDAL_OF_VECTOR | GDAL_OF_READONLY,
	                                                                 nullptr, nullptr, nullptr));
	if (inputDataset == nullptr)
		throw std::runtime_error("Error at opening the input file.");

	GDALDataset* referenceDataset = static_cast<GDALDataset*>(GDALOpenEx(referenceFile.c_str(),
	                                                                     GDAL_OF_VECTOR | GDAL_OF_READONLY,
	                                                                     nullptr, nullptr, nullptr));
	if (referenceDataset == nullptr)
		throw std::runtime_error("Error at opening the reference file.");

	OGRLayer* inputLayer, * referenceLayer;
	if (inputDataset->GetLayerCount() == 1)
		inputLayer = inputDataset->GetLayer(0);
	else
		throw std::runtime_error("There are more than 1 input layers.");

	if (referenceDataset->GetLayerCount() == 1)
		referenceLayer = referenceDataset->GetLayer(0);
	else
		throw std::runtime_error("There are more than 1 reference layers.");

	VectorMetadata inputMetadata(std::vector<OGRLayer*>{inputLayer});
	VectorMetadata referenceMetadata(std::vector<OGRLayer*>{referenceLayer});

	// Create bounding box for input dataset
	OGRGeometry* inputBoundingBox;
	{
		OGRMultiPoint inputBoundingPoints;
		OGRPoint inputBoundingPoint;

		inputBoundingPoint = OGRPoint(inputMetadata.originX(), inputMetadata.originY());
		inputBoundingPoints.addGeometry(&inputBoundingPoint);

		inputBoundingPoint = OGRPoint(inputMetadata.originX(), inputMetadata.originY() - inputMetadata.extentY());
		inputBoundingPoints.addGeometry(&inputBoundingPoint);

		inputBoundingPoint = OGRPoint(inputMetadata.originX() + inputMetadata.extentX(), inputMetadata.originY());
		inputBoundingPoints.addGeometry(&inputBoundingPoint);

		inputBoundingPoint = OGRPoint(inputMetadata.originX() + inputMetadata.extentX(),
		                              inputMetadata.originY() - inputMetadata.extentY());
		inputBoundingPoints.addGeometry(&inputBoundingPoint);

		inputBoundingBox = inputBoundingPoints.ConvexHull();
	}

	// Display metadata
	if (vm.count("verbose"))
	{
		std::cout << std::endl << "--- Input file ---" << std::endl;
		std::cout << "File path: \t" << inputFile << std::endl;
		std::cout << "Tree count: \t" << inputLayer->GetFeatureCount() << std::endl;
		std::cout << inputMetadata << std::endl;

		std::cout << std::endl << "--- Reference file ---" << std::endl;
		std::cout << "File path: \t" << referenceFile << std::endl;
		std::cout << "Tree count: \t" << referenceLayer->GetFeatureCount() << std::endl;
		std::cout << referenceMetadata << std::endl;

		if (!readBoolean("Would you like to continue?"))
		{
			std::cerr << "Operation aborted." << std::endl;
			return UserAbort;
		}
	}

	// Read input trees
	OGRFeature* feature;
	OGRGeometry* geometry;
	OGRPoint* point;
	OGRCoordinateTransformation* transformation;

	std::vector<OGRPoint> inputTrees;
	inputTrees.reserve(inputLayer->GetFeatureCount());
	while ((feature = inputLayer->GetNextFeature()) != nullptr)
	{
		geometry = feature->GetGeometryRef();
		if (geometry->getGeometryType() != wkbPoint && geometry->getGeometryType() != wkbPoint25D)
			throw std::runtime_error("A geometry is not a point.");

		point = (OGRPoint*) geometry;
		inputTrees.push_back(*point);
		OGRFeature::DestroyFeature(feature);
	}

	// Create CRS transformation (if required)
	if (!referenceMetadata.reference().IsSame(&inputMetadata.reference()))
	{
		if (vm.count("verbose"))
			std::cout << "Reprojection between input and reference dataset required." << std::endl;
		
		transformation = OGRCreateCoordinateTransformation(
			&referenceMetadata.reference(), &inputMetadata.reference());

		if (transformation == nullptr)
			throw std::runtime_error("Coordinate reference transformation failure.");
	}

	// Read reference trees
	std::vector<Tree> referenceTrees;
	while ((feature = referenceLayer->GetNextFeature()) != nullptr)
	{
		int year = feature->GetFieldAsInteger(yearField.c_str());
		int radius = feature->GetFieldAsInteger(radiusField.c_str());

		geometry = feature->GetGeometryRef();
		if (geometry->getGeometryType() != wkbPoint)
			throw std::runtime_error("A geometry is not a point.");
		point = (OGRPoint*) geometry;

		if (year >= minYear && year <= maxYear && radius >= minRadius)
		{
			double x = point->getX();
			double y = point->getY();
			
			if (transformation != nullptr)
			{
				if (!transformation->Transform(1, &x, &y))
					throw std::runtime_error("Coordinate reference transformation failure.");
			}

			OGRPoint transformedPoint(x, y);
			if (transformedPoint.Within(inputBoundingBox))
				referenceTrees.push_back(Tree{transformedPoint, year, radius});

		}

		OGRFeature::DestroyFeature(feature);
	}

	if (transformation != nullptr)
		delete transformation;

	if (vm.count("verbose"))
		std::cout << "Reference tree count (considered): " << referenceTrees.size() << std::endl;

	// Close datasets
	GDALClose(inputDataset);
	GDALClose(referenceDataset);
	delete inputBoundingBox;

	reporter->reset();
	reporter->report(0.f, "Verification");

	// Verification: matching input and reference trees
	bool found;
	unsigned int counter = 0;
	std::vector<Tree> matched, missed;
	for (const auto& referenceTree : referenceTrees)
	{
		found = false;
		for (const auto& inputTreeLocation : inputTrees)
		{
			if (referenceTree.location.Distance(&inputTreeLocation) <= std::max(referenceTree.radius, (int)minTolerance))
			{
				found = true;
				break;
			}
		}

		if (found)
			matched.push_back(referenceTree);
		else
			missed.push_back(referenceTree);

		++counter;
		if ((counter) % 100 == 0)
			reporter->report(counter * 1.f / referenceTrees.size(), "Verification");
	}
	reporter->report(1.f, "Verification");
	delete reporter;

	// Write result GeoJSON output file
	writeResultsToFile(matched, missed, inputMetadata, "result.json");

	// Write results to standard output
	std::cout << std::endl
	          << "Verification completed!" << std::endl
	          << std::endl << std::fixed << std::setprecision(2)
	          << "[Basic statistic]" << std::endl
	          << "Reference trees matched: " << matched.size() << std::endl
	          << "Reference trees failed: " << missed.size() << std::endl
	          << "Match ratio: " << ((100.f * matched.size()) / (referenceTrees.size())) << "%" << std::endl
	          << std::endl
	          << "[Miss statistic]" << std::endl
	          << "Average radius: " << ((std::accumulate(missed.begin(), missed.end(),
	                                                     0.0, [](double sum, const Tree& tree)
	                                                     {
		                                                     return sum + tree.radius;
	                                                     })) / missed.size()) << std::endl
	          << std::endl
	          << "[Advanced statistic]" << std::endl
	          << "Extraction rate: " << ((100.f * inputTrees.size()) / referenceTrees.size()) << "%" << std::endl
	          << "Matching rate: " << ((100.f * matched.size()) / (referenceTrees.size())) << "%" << std::endl
	          << "Commission rate: " << ((100.f * (inputTrees.size() - matched.size())) / inputTrees.size()) << "%" << std:: endl
	          << "Omission rate: " << ((100.f * missed.size()) / (referenceTrees.size())) << "%" << std::endl;

	// Execution time measurement
	std::clock_t clockEnd = std::clock();
	auto timeEnd = std::chrono::high_resolution_clock::now();

	std::cout << std::endl
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

void writeResultsToFile(
	const std::vector<Tree>& matched,
	const std::vector<Tree>& missed,
	VectorMetadata metadata,
	const std::string& outPath)
{
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
	if (driver == nullptr)
		throw std::invalid_argument("Target output format unrecognized.");

	if (fs::exists(outPath) &&
	    driver->Delete(outPath.c_str()) == CE_Failure &&
	    !fs::remove(outPath))
		throw std::runtime_error("Cannot overwrite previously created target file.");

	GDALDataset* ds = driver->Create(outPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
	if (ds == nullptr)
		throw std::runtime_error("Target file creation failed.");

	OGRLayer* layer = ds->CreateLayer("points", &metadata.reference(), wkbPoint, nullptr);
	{
		OGRFieldDefn field("Category", OFTString);
		field.SetWidth(10);
		if (layer->CreateField(&field) != OGRERR_NONE)
			throw std::runtime_error("Category field creation failed.");
	}
	{
		OGRFieldDefn field("Radius", OFTInteger);
		if (layer->CreateField(&field) != OGRERR_NONE)
			throw std::runtime_error("Category field creation failed.");
	}
	{
		OGRFieldDefn field("Year", OFTInteger);
		if (layer->CreateField(&field) != OGRERR_NONE)
			throw std::runtime_error("Category field creation failed.");
	}

	OGRFeature *feature;
	for(auto& tree : matched)
	{
		feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
		feature->SetField("Category", "matched");
		feature->SetField("Year", tree.year);
		feature->SetField("Radius", tree.radius);
		feature->SetGeometry(&tree.location);
		if (layer->CreateFeature(feature) != OGRERR_NONE)
			throw std::runtime_error("Feature creation failed.");
		OGRFeature::DestroyFeature(feature);
	}

	for(auto& tree : missed)
	{
		feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
		feature->SetField("Category", "missed");
		feature->SetField("Year", tree.year);
		feature->SetField("Radius", tree.radius);
		feature->SetGeometry(&tree.location);
		if (layer->CreateFeature(feature) != OGRERR_NONE)
			throw std::runtime_error("Feature creation failed.");
		OGRFeature::DestroyFeature(feature);
	}

	GDALClose(ds);
}
