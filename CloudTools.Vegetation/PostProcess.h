#pragma once

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

namespace CloudTools
{
namespace Vegetation
{
class PostProcess : public CloudTools::Operation
{
public:
	enum DifferenceMethod
	{
		Hausdorff,
		Centroid
	};

	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

protected:
	/// <summary>
	/// Internal progress reporter piped to override message.
	/// </summary>
	ProgressType _progress;
	/// <summary>
	/// The current progress message to report.
	/// </summary>
	std::string _progressMessage;

public:
	PostProcess(const std::string& dsmInputPathA, const std::string& dsmInputPathB,
	            const CloudTools::DEM::ClusterMap& clusterMapA, const CloudTools::DEM::ClusterMap& clusterMapB,
	            const std::string& outputDir,
	            DifferenceMethod method = DifferenceMethod::Centroid)
		: _dsmInputPathA(dsmInputPathA), _dsmInputPathB(dsmInputPathB),
		  _clustersA(clusterMapA), _clustersB(clusterMapB),
		  _outputDir(outputDir),
		  _method(method)
	{
	}

protected:
	/// <summary>
	/// Verifies the configuration.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Produces the output file(s).
	/// </summary>
	void onExecute() override;

private:
	std::string _dsmInputPathA, _dsmInputPathB;
	CloudTools::DEM::ClusterMap _clustersA, _clustersB;
	std::string _outputDir;
	DifferenceMethod _method;
	CloudTools::DEM::RasterMetadata _rasterMetadata;

	void writeClusterPairsToFile(const std::string& outPath, std::shared_ptr<DistanceCalculation> distance);

	void writeClusterHeightsToFile(const std::string& outPath, std::shared_ptr<DistanceCalculation> distance);
};
} // Vegetation
} // CloudTools
