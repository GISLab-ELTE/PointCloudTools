#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Metadata.h>
#include <CloudTools.DEM/Rasterize.h>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/SweepLineTransformation.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::DEM;
using namespace CloudTools::IO;

int main(int argc, char* argv[]) try
{
	std::string inputFile;
	std::string referenceFile;

	unsigned int coverageExpansion = 2;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("input,i", po::value<std::string>(&inputFile),
			"cluster map file to verify")
		("reference,r", po::value<std::string>(&referenceFile),
		    "reference file to verify against")
		("help,h", "produce help message")
		;

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
	std::cout << "=== AHN Vegetation Filter Verifier ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

	BarReporter reporter;
	GDALAllRegister();

	GDALDataset* inputDataset = static_cast<GDALDataset*>(GDALOpen(inputFile.c_str(), GA_ReadOnly));
	if (inputDataset == nullptr)
		throw std::runtime_error("Error at opening the input file.");

	GDALDataset* referenceDataset = static_cast<GDALDataset*>(GDALOpenEx(referenceFile.c_str(),
	                                                                     GDAL_OF_VECTOR | GDAL_OF_READONLY,
	                                                                     nullptr, nullptr, nullptr));
	if (referenceDataset == nullptr)
		throw std::runtime_error("Error at opening the reference file.");

	OGRLayer* layer;
	if (referenceDataset->GetLayerCount() == 1)
		layer = referenceDataset->GetLayer(0);
	else
		throw std::runtime_error("There are more than 1 layers.");

	RasterMetadata inputMetadata(inputDataset);
	VectorMetadata referenceMetadata(std::vector<OGRLayer*> { layer });

	GDALRasterBand* band = inputDataset->GetRasterBand(1);

	std::cout << layer->GetFeatureCount() << std::endl;

	OGRFeature *feature;
	OGRGeometry* geometry;
	OGRPoint* point;
	OGRCoordinateTransformation* transformation;
	while ((feature = layer->GetNextFeature()) != nullptr)
	{
		int year = feature->GetFieldAsInteger("Plantjaar");

		geometry = feature->GetGeometryRef();

		if (geometry->getGeometryType() != wkbPoint)
			throw std::runtime_error("A geometry is not a point.");

		point = (OGRPoint*) geometry;

		if (year > 0 && year <= 2015)
		{
			std::cout << year << std::endl;
			std::cout << point->getX() << ", " << point->getY() << std::endl;

			transformation = OGRCreateCoordinateTransformation(
				&referenceMetadata.reference(), &inputMetadata.reference());

			double x = point->getX();
			double y = point->getY();
			if (transformation == nullptr || !transformation->Transform(1, &x, &y))
				throw std::runtime_error("Coordinate reference transformation failure.");
			std::cout << x << ", " << y << std::endl;

			delete transformation;
		}

		OGRFeature::DestroyFeature(feature);
	}

	GDALClose(inputDataset);
	GDALClose(referenceDataset);

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
