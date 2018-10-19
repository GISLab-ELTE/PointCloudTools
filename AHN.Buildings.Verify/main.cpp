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

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Metadata.h>
#include <CloudTools.DEM/Rasterize.h>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/SweepLineTransformation.hpp>
#include "Coverage.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace CloudTools::DEM;
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

	unsigned int coverageExpansion = 2;

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
		("coverage-expansion", po::value<unsigned int>(&coverageExpansion)->default_value(coverageExpansion),
			"expansion of coverage in meters for overlay correction")
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
	unsigned long approvedBasicCount = 0,
		          rejectedBasicCount = 0,
		          approvedCorrectedCount = 0,
		          rejectedCorrectedCount = 0;
	double approvedBasicSum = 0,
		   rejectedBasicSum = 0,
		   approvedCorrectedSum = 0,
		   rejectedCorrectedSum = 0;

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

			std::vector<GDALDataset*> references;
			references.reserve(listReferences.size());

			// Computation mark points are the lower boundaries of the processes
			std::map<std::string, std::size_t> computationMark =
			{
				{"basic", listReferences.size()},
				{"correctedBinarization", listReferences.size() + 1},
				{"correctedCoverage", listReferences.size() + 2},
				{"correctedExpansion", listReferences.size() + 3},
				{"correctedCalculation", listReferences.size() + 3 + 2 * coverageExpansion},
			};
			std::size_t computationSteps = std::max_element(computationMark.begin(), computationMark.end(),
				[](const std::map<std::string, std::size_t>::value_type& a, const std::map<std::string, std::size_t>::value_type& b)
			{
				return a.second < b.second;
			})->second + 1;

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

				rasterizer.progress = [&reporter, i, computationSteps](float complete, const std::string &message)
				{
					reporter.report(1.f * i / computationSteps + complete / computationSteps, message);
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
				catch (std::logic_error&) // no overlap
				{
					reporter.report(1.f * (i + 1) / computationSteps);
					continue;
				}
				rasterizer.execute();
				references.push_back(rasterizer.target());
			}

			std::vector<GDALDataset*> sources(references.size() + 1);
			sources[0] = ahnDataset;
			std::copy(references.begin(), references.end(), sources.begin() + 1);

#pragma region Basic AHN altimetry change location verification
			{
				// Operation definition
				SweepLineCalculation<float> verification(sources, 0,
					[&approvedBasicCount, &rejectedBasicCount, &approvedBasicSum, &rejectedBasicSum]
				(int x, int y, const std::vector<Window<float>>& data)
				{
					const auto& ahn = data[0];
					if (!ahn.hasData()) return;

					for (int i = 1; i < data.size(); ++i)
						if (data[i].hasData())
						{
							++approvedBasicCount;
							approvedBasicSum += std::abs(ahn.data());
							return;
						}
					++rejectedBasicCount;
					rejectedBasicSum += std::abs(ahn.data());
				},
					[&reporter, &computationMark, computationSteps](float complete, const std::string &message)
				{
					reporter.report(1.f * (computationMark["basic"]) / computationSteps + complete / computationSteps, message);
					return true;
				});
				verification.spatialReference = "EPSG:28992";

				// Execute operation
				verification.execute();
			}
#pragma endregion

#pragma region Corrected AHN altimetry change location verification
			// AHN binarization
			GDALDataset* ahnCoverage;
			{
				SweepLineTransformation<GByte, float> binarization({ ahnDataset }, 0, 
					[](int x, int y, const std::vector<Window<float>>& data)
				{
					const auto& ahn = data[0];
					return ahn.hasData() ? Coverage::Accept : Coverage::NoData;
				},
					[&reporter, &computationMark, computationSteps](float complete, const std::string &message)
				{
					reporter.report(1.f * (computationMark["correctedBinarization"]) / computationSteps + complete / computationSteps, message);
					return true;
				});
				binarization.nodataValue = Coverage::NoData;

				// Execute operation
				binarization.execute();
				ahnCoverage = binarization.target();
			}

			// AHN coverage with reference data
			sources[0] = ahnCoverage;
			{
				SweepLineTransformation<GByte> coverage({ sources }, 0, 
					[](int x, int y, const std::vector<Window<GByte>>& data)
				{
					const auto& ahn = data[0];
					if (!ahn.hasData()) return Coverage::NoData;

					for (int i = 1; i < data.size(); ++i)
						if (data[i].hasData())
							return Coverage::Accept;
					return Coverage::Reject;
				},
					[&reporter, &computationMark, computationSteps](float complete, const std::string &message)
				{
					reporter.report(1.f * (computationMark["correctedCoverage"]) / computationSteps + complete / computationSteps, message);
					return true;
				});
				coverage.nodataValue = Coverage::NoData;
				coverage.spatialReference = "EPSG:28992";

				// Execute operation
				coverage.execute();
				GDALClose(ahnCoverage);
				ahnCoverage = coverage.target();
			}

			// Iterative coverage expansion
			unsigned int iterations = 0;
			for(; iterations < 2 * coverageExpansion; ++iterations)
			{
				unsigned int change = 0;
				SweepLineTransformation<GByte> expansion({ ahnCoverage }, 1, 
					[&change](int x, int y, const std::vector<Window<GByte>>& data)
				{
					const auto& coverage = data[0];
					if (!coverage.hasData()) return Coverage::NoData;

					if(coverage.data() == Coverage::Reject)
					{
						if (coverage.data(-1, 0) == Coverage::Accept ||
							coverage.data(1, 0)  == Coverage::Accept ||
							coverage.data(0, -1) == Coverage::Accept ||
							coverage.data(0, 1)  == Coverage::Accept)
						{
							++change;
							return Coverage::Accept;
						}
						else
							return Coverage::Reject;
					}
					return Coverage::Accept;
				},
					[&reporter, &computationMark, computationSteps, iterations](float complete, const std::string &message)
				{
					reporter.report(1.f * (computationMark["correctedExpansion"] + iterations) / computationSteps + complete / computationSteps, message);
					return true;
				});
				expansion.nodataValue = Coverage::NoData;
				expansion.spatialReference = "EPSG:28992";

				// Execute operation
				expansion.execute();
				GDALClose(ahnCoverage);
				ahnCoverage = expansion.target();

				// Break if no expansion
				if (change == 0) break;
			}

			// Calculate corrected verification
			{
				SweepLineCalculation<float> calculation(std::vector<GDALDataset*>{ ahnDataset, ahnCoverage }, 0,
					[&approvedCorrectedCount, &rejectedCorrectedCount, &approvedCorrectedSum, &rejectedCorrectedSum]
				(int x, int y, const std::vector<Window<float>>& data)
				{
					const auto& ahn = data[0];
					const auto& coverage = data[1];
					if (!ahn.hasData()) return;

					if (coverage.data() == Coverage::Accept)
					{
						++approvedCorrectedCount;
						approvedCorrectedSum += std::abs(ahn.data());
					}
					else
					{
						++rejectedCorrectedCount;
						rejectedCorrectedSum += std::abs(ahn.data());
					}
				},
					[&reporter, &computationMark, computationSteps](float complete, const std::string &message)
				{
					reporter.report(1.f * (computationMark["correctedCalculation"]) / computationSteps + complete / computationSteps, message);
					return true;
				});
				calculation.spatialReference = "EPSG:28992";

				// Execute operation
				calculation.execute();
			}
			reporter.report(1.f);
#pragma endregion

			// Close reference datasets
			for (GDALDataset *dataset : references)
				GDALClose(dataset);

			// Close AHN datasets
			GDALClose(ahnDataset);
			GDALClose(ahnCoverage);
		}
	}

	// Write results to standard output
	std::cout << std::endl
		<< "All completed!" << std::endl 
		<< std::endl << std::fixed << std::setprecision(2)
		<< "[Basic]" << std::endl
		<< "Approved count: " << approvedBasicCount << std::endl
		<< "Approved sum: " << approvedBasicSum << std::endl
		<< "Rejected count: " << rejectedBasicCount << std::endl
		<< "Rejected sum: " << rejectedBasicSum<< std::endl
		<< "Ratio by count: " << ((100.f * approvedBasicCount) / (approvedBasicCount + rejectedBasicCount)) << "%" << std::endl
		<< "Ratio by sum: " << ((100.f * approvedBasicSum) / (approvedBasicSum + rejectedBasicSum)) << "%" << std::endl
		<< std::endl
		<< "[Corrected]" << std::endl
		<< "Approved count: " << approvedCorrectedCount << std::endl
		<< "Approved sum: " << approvedCorrectedSum << std::endl
		<< "Rejected count: " << rejectedCorrectedCount << std::endl
		<< "Rejected sum: " << rejectedCorrectedSum << std::endl
		<< "Ratio by count: " << ((100.f * approvedCorrectedCount) / (approvedCorrectedCount + rejectedCorrectedCount)) << "%" << std::endl
		<< "Ratio by sum: " << ((100.f * approvedCorrectedSum) / (approvedCorrectedSum + rejectedCorrectedSum)) << "%" << std::endl;

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
