#include <iostream>
#include <string>
#include <utility>

#include <boost/program_options.hpp>
#include <gdal_priv.h>

#include <CloudTools.Common/IO/IO.h>
#include <CloudTools.Common/IO/Reporter.h>
#include <CloudTools.DEM/Comparers/Difference.hpp>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/Filters/MorphologyFilter.hpp>
#include "NoiseFilter.h"
#include "MatrixTransformation.h"
#include "TreeCrownSegmentation.h"
#include "MorphologyClusterFilter.hpp"

namespace po = boost::program_options;

using namespace CloudTools::IO;
using namespace CloudTools::DEM;
using namespace AHN::Vegetation;

int main(int argc, char* argv[])
{
	std::string DTMinputPath;
	std::string DSMinputPath;
	std::string AHN2DSMinputPath;
	std::string AHN2DTMinputPath;
	std::string outputPath = (fs::current_path() / "out.tif").string();
	std::string AHN2outputPath = (fs::current_path() / "ahn2_out.tif").string();

	// Read console arguments
	po::options_description desc("Allowed options");
	desc.add_options()
		("ahn3-dtm-input-path,t", po::value<std::string>(&DTMinputPath), "DTM input path")
		("ahn3-dsm-input-path,s", po::value<std::string>(&DSMinputPath), "DSM input path")
		("ahn2-dtm-input-path,y", po::value<std::string>(&AHN2DTMinputPath), "AHN2 DTM input path")
		("ahn2-dsm-input-path,x", po::value<std::string>(&AHN2DSMinputPath), "AHN2 DSM input path")
		("output-path,o", po::value<std::string>(&outputPath)->default_value(outputPath), "output path")
		("verbose,v", "verbose output")
		("quiet,q", "suppress progress output")
		("help,h", "produce help message");

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
	if (!vm.count("ahn3-dsm-input-path"))
	{
		std::cerr << "Surface input file is mandatory." << std::endl;
		argumentError = true;
	}

	if (!vm.count("ahn3-dtm-input-path"))
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
	std::cout << "CHM generated." << std::endl;

	// Count local maximum values in CHM
	SweepLineCalculation<float> *countLocalMax = new SweepLineCalculation<float>(
		{ comparison->target() }, 1, nullptr);

	int counter = 0;
	countLocalMax->computation = [&countLocalMax, &counter](int x, int y, const std::vector<Window<float>> &sources)
	{
		const Window<float> &source = sources[0];
		if (!source.hasData())
			return;

		for (int i = -countLocalMax->range(); i <= countLocalMax->range(); i++)
			for (int j = -countLocalMax->range(); j <= countLocalMax->range(); j++)
				if (source.data(i, j) > source.data(0, 0))
					return;
		++counter;
	};

	if (!vm.count("quiet"))
	{
		countLocalMax->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	countLocalMax->execute();
	std::cout << "Number of local maximums are: " << counter << std::endl;


	// Vegetation filter
	/*NoiseFilter* filter = new NoiseFilter(comparison->target(), outputPath, 3);
	if (!vm.count("quiet"))
	{
		filter->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	filter->execute();*/

	// Perform anti-aliasing via convolution matrix
	MatrixTransformation *filter = new MatrixTransformation(comparison->target(), "antialias.tif", 1);
	if (!vm.count("quiet"))
	{
		filter->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	filter->execute();
	std::cout << "Matrix transformation performed." << std::endl;

	// Eliminate all points that are shorter than a possible tree
	float thresholdOfTrees = 1.5;
	SweepLineTransformation<float> *eliminateNonTrees = new SweepLineTransformation<float>(
		{ filter->target() }, "nosmall.tif", 0, nullptr);

	eliminateNonTrees->computation = [&eliminateNonTrees, &thresholdOfTrees](int x, int y,
		const std::vector<Window<float>> &sources)
	{
		const Window<float> &source = sources[0];
		if (!source.hasData() || source.data() < thresholdOfTrees)
			return static_cast<float>(eliminateNonTrees->nodataValue);
		else
			return source.data();
	};

	if (!vm.count("quiet"))
	{
		eliminateNonTrees->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	eliminateNonTrees->execute();
	std::cout << "Too small values eliminated." << std::endl;

	// Count & collect local maximum values
	SweepLineCalculation<float> *collectSeeds = new SweepLineCalculation<float>(
		{ eliminateNonTrees->target() }, 4, nullptr);

	counter = 0;
	std::vector<OGRPoint> seedPoints;
	collectSeeds->computation = [&collectSeeds, &counter, &seedPoints](int x, int y,
		const std::vector<Window<float>> &sources)
	{
		const Window<float> &source = sources[0];
		if (!source.hasData())
			return;

		for (int i = -collectSeeds->range(); i <= collectSeeds->range(); i++)
			for (int j = -collectSeeds->range(); j <= collectSeeds->range(); j++)
				if (source.data(i, j) > source.data(0, 0))
					return;
		++counter;
		seedPoints.emplace_back(x, y);
	};

	if (!vm.count("quiet"))
	{
		collectSeeds->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	collectSeeds->execute();
	std::cout << "Number of local maximums are: " << counter << std::endl;

	ClusterMap clusters;
	// Tree crown segmentation
	TreeCrownSegmentation *crownSegmentation = new TreeCrownSegmentation(
		{ eliminateNonTrees->target() }, seedPoints, nullptr);
	if (!vm.count("quiet"))
	{
		crownSegmentation->progress = [&reporter](float complete, const std::string &message)
		{
			reporter->report(complete, message);
			return true;
		};
	}
	reporter->reset();
	crownSegmentation->execute();
	std::cout << "Tree crown segmentation performed." << std::endl;

    MorphologyClusterFilter *morphologyFilterErosion = new MorphologyClusterFilter(
        clusters, "erosion.tif", MorphologyClusterFilter::Method::Erosion, nullptr);
    morphologyFilterErosion->threshold = 6;
    /*if (!vm.count("quiet"))
    {
      morphologyFilterErosion->progress = [&reporter](float complete, const std::string &message)
      {
        reporter->report(complete, message);
        return true;
      };
    }*/
    reporter->reset();
    morphologyFilterErosion->execute();
    std::cout << "Morphological erosion performed." << std::endl;

    MorphologyClusterFilter *morphologyFilterDilation = new MorphologyClusterFilter(
        clusters, outputPath, MorphologyClusterFilter::Method::Dilation, nullptr);
    /*if (!vm.count("quiet"))
    {
      morphologyFilterDilation->progress = [&reporter](float complete, const std::string &message)
      {
        reporter->report(complete, message);
        return true;
      };
    }*/
    reporter->reset();
    morphologyFilterDilation->execute();
    std::cout << "Morphological dilation performed." << std::endl;

	if (vm.count("ahn2-dtm-input-path") && vm.count("ahn2-dsm-input-path"))
	{
		// CHM
		Difference<float> *ahn2comparison = new Difference<float>({ AHN2DTMinputPath, AHN2DSMinputPath }, "ahn2_CHM.tif");
		if (!vm.count("quiet"))
		{
			ahn2comparison->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}
		ahn2comparison->execute();
		std::cout << "AHN2 CHM generated." << std::endl;

		// Anti-aliasing
		MatrixTransformation *ahn2filter = new MatrixTransformation(ahn2comparison->target(), "ahn2_antialias.tif", 1);
		if (!vm.count("quiet"))
		{
			ahn2filter->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}
		reporter->reset();
		ahn2filter->execute();
		std::cout << "AHN2 Matrix transformation performed." << std::endl;

		// Eliminate small values
		SweepLineTransformation<float> *ahn2eliminateNonTrees = new SweepLineTransformation<float>(
			{ ahn2filter->target() }, "ahn2_nosmall.tif", 0, nullptr);

		ahn2eliminateNonTrees->computation = [&ahn2eliminateNonTrees, &thresholdOfTrees](int x, int y,
			const std::vector<Window<float>> &sources)
		{
			const Window<float> &source = sources[0];
			if (!source.hasData() || source.data() < thresholdOfTrees)
				return static_cast<float>(ahn2eliminateNonTrees->nodataValue);
			else
				return source.data();
		};

		if (!vm.count("quiet"))
		{
			ahn2eliminateNonTrees->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}
		reporter->reset();
		ahn2eliminateNonTrees->execute();
		std::cout << "AHN2 Too small values eliminated." << std::endl;

		// Collect seeds
		SweepLineCalculation<float> *ahn2collectSeeds = new SweepLineCalculation<float>(
			{ ahn2eliminateNonTrees->target() }, 4, nullptr);

		counter = 0;
		std::vector<OGRPoint> ahn2seedPoints;
		ahn2collectSeeds->computation = [&ahn2collectSeeds, &counter, &ahn2seedPoints](int x, int y,
			const std::vector<Window<float>> &sources)
		{
			const Window<float> &source = sources[0];
			if (!source.hasData())
				return;

			for (int i = -ahn2collectSeeds->range(); i <= ahn2collectSeeds->range(); i++)
				for (int j = -ahn2collectSeeds->range(); j <= ahn2collectSeeds->range(); j++)
					if (source.data(i, j) > source.data(0, 0))
						return;
			++counter;
			ahn2seedPoints.emplace_back(x, y);
		};

		if (!vm.count("quiet"))
		{
			ahn2collectSeeds->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}
		reporter->reset();
		ahn2collectSeeds->execute();
		std::cout << "AHN2 Number of local maximums are: " << counter << std::endl;

		ClusterMap clusterMap;
		// Tree crown segmentation
		TreeCrownSegmentation *ahn2crownSegmentation = new TreeCrownSegmentation(
			{ ahn2eliminateNonTrees->target() }, seedPoints, nullptr);
		if (!vm.count("quiet"))
		{
			ahn2crownSegmentation->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}
		reporter->reset();
		ahn2crownSegmentation->execute();
		std::cout << "AHN2 Tree crown segmentation performed." << std::endl;

		// Morphological opening
		MorphologyClusterFilter *ahn2morphologyFilterErosion = new MorphologyClusterFilter(
			clusterMap, "ahn2_erosion.tif", MorphologyClusterFilter::Method::Erosion, nullptr);
		ahn2morphologyFilterErosion->threshold = 6;
		/*if (!vm.count("quiet"))
		{
			ahn2morphologyFilterErosion->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}*/
		reporter->reset();
		ahn2morphologyFilterErosion->execute();
		std::cout << "AHN2 Morphological erosion performed." << std::endl;

		MorphologyClusterFilter *ahn2morphologyFilterDilation = new MorphologyClusterFilter(
			clusterMap, AHN2outputPath, MorphologyClusterFilter::Method::Dilation, nullptr);
		/*if (!vm.count("quiet"))
		{
			ahn2morphologyFilterDilation->progress = [&reporter](float complete, const std::string &message)
			{
				reporter->report(complete, message);
				return true;
			};
		}*/
		reporter->reset();
		ahn2morphologyFilterDilation->execute();
		std::cout << "AHN2 Morphological dilation performed." << std::endl;

		delete ahn2morphologyFilterDilation;
		delete ahn2morphologyFilterErosion;
		delete ahn2crownSegmentation;
		delete ahn2collectSeeds;
		delete ahn2eliminateNonTrees;
		delete ahn2filter;
		delete ahn2comparison;
	}

    delete morphologyFilterDilation;
	delete morphologyFilterErosion;
    delete crownSegmentation;
	delete countLocalMax;
	delete comparison;
	delete filter;
	delete collectSeeds;
	delete reporter;
	return Success;
}

bool reporter(CloudTools::DEM::SweepLineTransformation<float> *transformation, CloudTools::IO::Reporter *reporter)
{
	transformation->progress = [&reporter](float complete, const std::string &message)
	{
		reporter->report(complete, message);
		return true;
	};
	return false;
}