#pragma once

#include <CloudTools.Common/Operation.h>
#include <CloudTools.Common/IO/ResultCollection.h>
#include <CloudTools.DEM/Metadata.h>
#include <CloudTools.DEM/ClusterMap.h>

namespace CloudTools
{
namespace Vegetation
{
class PreProcess : public CloudTools::Operation, protected CloudTools::IO::ResultCollection
{
public:
	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

	/// <summary>
	/// Iteration steps of morphology operations.
	/// </summary>
	unsigned int morphologyCounter = 3;

	/// <summary>
	/// Threshold value for morphological erosion.
	/// </summary>
	unsigned int erosionThreshold = 6;

	/// <summary>
	/// Remove clusters smaller than this threshold.
	/// </summary>
	unsigned int removalRadius = 16;

	/// <summary>
	/// Keep intermediate results on disk after progress.
	/// </summary>
	bool debug = false;

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
	PreProcess(const std::string& prefix,
	           const std::string& dtmInputPath,
	           const std::string& dsmInputPath,
	           const std::string& outputDir)
		: _prefix(prefix),
		  _dtmInputPath(dtmInputPath),
		  _dsmInputPath(dsmInputPath),
		  _outputDir(outputDir)
	{
	}

	inline CloudTools::DEM::ClusterMap& target()
	{
		if (!isExecuted())
			throw std::logic_error("The operation is not executed.");
		return _targetCluster;
	}

	inline CloudTools::DEM::RasterMetadata targetMetadata()
	{
		if (!isExecuted())
			throw std::logic_error("The operation is not executed.");
		return _targetMetadata;
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

	/// <summary>
	/// Creates a new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>New result on the heap.</returns>
	CloudTools::IO::Result* createResult(const std::string& name, bool isFinal = false) override;

private:
	std::string _prefix, _dtmInputPath, _dsmInputPath, _outputDir;
	CloudTools::DEM::RasterMetadata _targetMetadata;
	CloudTools::DEM::ClusterMap _targetCluster;

	/// <summary>
	/// Applies blurring convolution. 3x3 Gaussian kernel.
	/// </summary>
	/// <remarks>
	/// Kernel matrix:
	/// [ 1 2 1
	///   2 4 2
	///   1 2 1 ]
	/// </remarks>
	/// <param name="sourceDataset">The source dataset of the matrix transformation.</param>
	/// <param name="targetPath">The target file of the  matrix transformation.</param>
	GDALDataset* blur3x3Middle4(GDALDataset* sourceDataset, const std::string& targetPath);

	/// <summary>
	/// Applies blurring convolution. 3x3 Gaussian kernel.
	/// </summary>
	/// <remarks>
	/// Kernel matrix:
	/// [ 1  3 1
	///   3 12 3
	///   1  3 1 ]
	/// </remarks>
	/// <param name="sourceDataset">The source dataset of the matrix transformation.</param>
	/// <param name="targetPath">The target file of the  matrix transformation.</param>
	GDALDataset* blur3x3Middle12(GDALDataset* sourceDataset, const std::string& targetPath);

	/// <summary>
	/// Applies blurring convolution. 5x5 Gaussian kernel.
	/// </summary>
	/// <remarks>
	/// Kernel matrix:
	/// [ 1  4  6  4 1
	///   4 16 24 16 4
	///   6 24 36 24 6
	///   4 16 24 16 4
	///   1  4  6  4 1 ]
	/// </remarks>
	/// <param name="sourceDataset">The source dataset of the matrix transformation.</param>
	/// <param name="targetPath">The target file of the  matrix transformation.</param>
	GDALDataset* blur5x5Middle36(GDALDataset* sourceDataset, const std::string& targetPath);

	std::vector<OGRPoint> collectSeedPoints(GDALDataset* target);

	/// <summary>
	/// Removed deformed clusters from the cluster map.
	/// </summary>
	/// <remarks>
	/// A cluster is deemed deformed (as a tree), if one the following 3 conditions apply:
	/// - its X dimension is less than half of its Y dimension;
	/// - its Y dimension is less than half of its X dimension;
	/// - less than 60% of its bounding box belongs to the cluster.
	/// </remarks>
	/// <param name="clusterMap">The cluster map to transform.</param>
	void removeDeformedClusters(CloudTools::DEM::ClusterMap& clusterMap);

	void writePointsToFile(std::vector<OGRPoint> points, const std::string& outPath);

	void writeClusterMapToFile(const std::string& outPath);
};
} // Vegetation
} // CloudTools
