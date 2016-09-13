#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO.h>
#include <CloudTools.Common/Reporter.h>
#include <CloudLib.DEM/Metadata.h>
#include <CloudLib.DEM/Rasterize.h>
#include <CloudLib.DEM/SweepLineCalculation.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudLib::DEM;
using namespace CloudTools::IO;

int main(int argc, char* argv[]) try
{
	std::string ahnDir;
	std::string ahnPattern = ".*\\.tif";

	std::vector<std::string> fileReferences;
	std::vector<std::string> fileLayers;

	std::vector<std::string> dirReferences;
	std::vector<std::string> dirPatterns;
	std::vector<std::string> dirLayers;

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn-dir", po::value<std::string>(&ahnDir), 
			"directory path for AHN changeset tiles")
		("ahn-pattern", po::value<std::string>(&ahnPattern)->default_value(ahnPattern),
			"file pattern for AHN tiles")
		("file-reference", po::value<std::vector<std::string>>(&fileReferences), 
			"file path for reference vector files")
		("file-layer", po::value<std::vector<std::string>>(&fileLayers),
			"layer name for reference files")
		("dir-reference", po::value<std::vector<std::string>>(&dirReferences),
			"directory path for reference files")
		("dir-pattern", po::value<std::vector<std::string>>(&dirPatterns),
			"file pattern for reference directories")
		("dir-layer", po::value<std::vector<std::string>>(&dirLayers), 
			"layer name for reference directories")
		("help,h", "produce help message")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Argument validation
	if (vm.count("help"))
	{
		std::cout << "Verifies detected building changes against reference files." << std::endl;
		std::cout << desc << std::endl;
		return Success;
	}

	bool argumentError = false;
	if (!vm.count("ahn-dir"))
	{
		std::cerr << "AHN directory is mandatory." << std::endl;
		argumentError = true;
	}
	if (!fs::is_directory(ahnDir))
	{
		std::cerr << "The AHN directory does not exist." << std::endl;
		argumentError = true;
	}

	if (!vm.count("file-reference") && !vm.count("dir-reference"))
	{
		std::cerr << "At least 1 reference file or directory must be given." << std::endl;
		argumentError = true;
	}
	for (const std::string& path : fileReferences)
		if (!fs::is_regular_file(path))
		{
			std::cerr << "Reference file ('" << path << "') does not exist." << std::endl;
			argumentError = true;
		}
	for (const std::string& path : dirReferences)
		if (!fs::is_directory(path))
		{
			std::cerr << "Reference directory ('" << path << "') does not exist." << std::endl;
			argumentError = true;
		}

	if (vm.count("file-layer") > vm.count("file-reference"))
		std::cerr << "WARNING: more layer names given than reference files, ignoring the rest." << std::endl;
	if (vm.count("dir-layer") > vm.count("dir-reference"))
		std::cerr << "WARNING: more layer names given than reference directories, ignoring the rest." << std::endl;
	if (vm.count("dir-pattern") > vm.count("dir-reference"))
		std::cerr << "WARNING: more patterns given than reference directories, ignoring the rest." << std::endl;


	if (argumentError)
	{
		std::cerr << "Use the --help option for description." << std::endl;
		return InvalidInput;
	}

	// Program
	std::cout << "=== AHN Building Filter Verifier ===" << std::endl;
	std::clock_t clockStart = std::clock();
	auto timeStart = std::chrono::high_resolution_clock::now();

	BarReporter reporter;
	GDALAllRegister();
	unsigned long approvedCount = 0,
		          rejectedCount = 0;
	double approvedSum = 0,
		   rejectedSum = 0;

	// For each AHN tile
	boost::regex ahnRegex(ahnPattern);
	for (fs::directory_iterator ahnFile(ahnDir); ahnFile != fs::directory_iterator(); ++ahnFile)
	{
		if (fs::is_regular_file(ahnFile->status()) &&
			boost::regex_match(ahnFile->path().filename().string(), ahnRegex))
		{
			fs::path ahnPath = ahnFile->path();
			std::vector<std::string> listReferences;
			std::vector<std::string> listLayers;

			// Look for reference files
			for (unsigned int i = 0; i < fileReferences.size(); ++i)
			{
				listReferences.push_back(fileReferences[i]);
				if (fileLayers.size() > i)
					listLayers.push_back(fileLayers[i]);
				else
					listLayers.push_back(std::string());
			}

			// Look for files in reference directories
			for (unsigned int i = 0; i < dirReferences.size(); ++i)
			{
				// Criteria: file must have AHN group number in name
				boost::smatch groupMatch;
				if (!boost::regex_search(ahnFile->path().string(), groupMatch, boost::regex("[[:digit:]]{2}")))
					throw std::runtime_error("Unable to deduce the group number from AHN tile filename.");

				boost::regex groupRegex(".*" + groupMatch.str() + "[^_]*");
				boost::regex referenceRegex(".*");
				if (dirPatterns.size() > i)
					referenceRegex.assign(dirPatterns[i]);

				for (fs::directory_iterator referenceFile(dirReferences[i]);
					referenceFile != fs::directory_iterator();
					++referenceFile)
				{
					if (fs::is_regular_file(referenceFile->status()) &&
						boost::regex_match(referenceFile->path().stem().string(), groupRegex) &&
						boost::regex_match(referenceFile->path().filename().string(), referenceRegex))
					{
						listReferences.push_back(referenceFile->path().string());
						if (dirLayers.size() > i)
							listLayers.push_back(dirLayers[i]);
						else
							listLayers.push_back(std::string());
					}
				}
			}

			// Open AHN tile
			GDALDataset* ahnDataset = static_cast<GDALDataset*>(GDALOpen(ahnPath.string().c_str(), GA_ReadOnly));
			if (ahnDataset == nullptr)
				throw std::runtime_error("Error at opening the AHN tile.");
			RasterMetadata ahnMetadata(ahnDataset);

			std::vector<GDALDataset*> sources(1);
			sources[0] = ahnDataset;
			unsigned int computationStep = listReferences.size() + 1;

			// Write process header
			std::cout << std::endl
				<< "Processing tile: " << ahnPath.stem() << std::endl
				<< "Reference files found: " << std::endl;
			for (const std::string& path : listReferences)
				std::cout << '\t' << path << std::endl;
			reporter.reset();
			reporter.report(0.f);

			// Rasterize reference vector files
			for (unsigned int i = 0; i < listReferences.size(); ++i)
			{
				// Create the raster reference tile for the AHN tile
				Rasterize rasterizer(listReferences[i], "",
				                     !listLayers[i].empty() ? std::vector<std::string>{listLayers[i]} : std::vector<std::string>());
				rasterizer.targetFormat = "MEM";

				rasterizer.progress = [&reporter, i, computationStep](float complete, const std::string &message)
				{
					reporter.report(1.f * i / computationStep + complete / computationStep, message);
					return true;
				};

				// Define clipping for raster filter				
				rasterizer.pixelSizeX = ahnMetadata.pixelSizeX();
				rasterizer.pixelSizeY = ahnMetadata.pixelSizeY();
				rasterizer.clip(ahnMetadata.originX(), ahnMetadata.originY(),
					ahnMetadata.rasterSizeX(), ahnMetadata.rasterSizeY());

				// Execute operation
				try
				{
					rasterizer.prepare();
				}
				catch(std::logic_error&) // no overlap
				{
					reporter.report(1.f * (i + 1) / computationStep);
					continue;
				}
				rasterizer.execute();
				sources.push_back(rasterizer.target());
			}

			// AHN altimetry change location verification
			SweepLineCalculation<float> calculation(sources, 0,
				[&approvedCount, &rejectedCount, &approvedSum, &rejectedSum]
			(int x, int y, const std::vector<Window<float>>& data)
			{
				const auto& ahn = data[0];
				if (!ahn.hasData()) return;

				for (int i = 1; i < data.size(); ++i)
					if (data[i].hasData())
					{
						++approvedCount;
						approvedSum += std::abs(ahn.data());
						return;
					}
				++rejectedCount;
				rejectedSum += std::abs(ahn.data());
			},
				[&reporter, computationStep](float complete, const std::string &message)
			{
				reporter.report(1.f * (computationStep - 1) / computationStep + complete / computationStep, message);
				return true;
			});
			calculation.spatialReference = "EPSG:28992";

			// Execute operation
			calculation.execute();
			reporter.report(1.f);

			// Close datasets
			for (GDALDataset *dataset : sources)
				GDALClose(dataset);
		}
	}

	// Write results to standard output
	std::cout << std::endl
		<< "All completed!" << std::endl << std::fixed << std::setprecision(2)
		<< "Approved count: " << approvedCount << std::endl
		<< "Approved sum: " << approvedSum << std::endl
		<< "Rejected count: " << rejectedCount << std::endl
		<< "Rejected sum: " << rejectedSum<< std::endl
		<< "Ratio by count: " << ((100.f * approvedCount) / (approvedCount + rejectedCount)) << "%" << std::endl
		<< "Ratio by sum: " << ((100.f * approvedSum) / (approvedSum + rejectedSum)) << "%" << std::endl;

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
