#pragma once

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>

#include "DistanceCalculation.h"

namespace AHN
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
	PostProcess(const std::string& ahn2DSMInputPath, const std::string& ahn3DSMInputPath,
	            const CloudTools::DEM::ClusterMap& clusterMapAHN2, const CloudTools::DEM::ClusterMap& clusterMapAHN3,
	            const std::string& outputDir,
	            DifferenceMethod method = DifferenceMethod::Centroid)
		: _ahn2DSMInputPath(ahn2DSMInputPath), _ahn3DSMInputPath(ahn3DSMInputPath),
		  _clustersAHN2(clusterMapAHN2), _clustersAHN3(clusterMapAHN3),
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
	std::string _ahn2DSMInputPath, _ahn3DSMInputPath;
	CloudTools::DEM::ClusterMap _clustersAHN2, _clustersAHN3;
	std::string _outputDir;
	DifferenceMethod _method;
	CloudTools::DEM::RasterMetadata _rasterMetadata;

	void writeClusterPairsToFile(const std::string& outPath, std::shared_ptr<DistanceCalculation> distance);

	void writeClusterHeightsToFile(const std::string& outPath, std::shared_ptr<DistanceCalculation> distance);
};
} // Vegetation
} // AHN
