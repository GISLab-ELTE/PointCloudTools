#pragma once

#include <string>

#include <CloudLib.DEM/Transformation.h>
#include <CloudLib.DEM/SweepLineTransformation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents a cluster filter for DEM datasets.
/// </summary>
class ClusterFilter : public CloudLib::DEM::Transformation
{
public:	
	/// <summary>
	/// Cluster size threshold in pixels.
	/// </summary>
	/// <remarks>
	/// 400 pixels is 100m2 with 0.5m AHN raster grid.
	/// </remarks>
	int sizeThreshold = 400;	
	/// <summary>
	/// <c>true</c> to test connectedness diagonally, <c>false</c> otherwise.
	/// </summary>
	bool diagonalConnectedness = false;

protected:
	std::string _sievePath;
	GDALDataset* _sieveDataset;
	bool _sieveOwnerShip = true;

	ProgressType _progressBinarization;
	ProgressType _progressSieve;
	ProgressType _progressFilter;

public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="filterPath">The filter map file of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="progress">The callback method to report progress.</param>
	ClusterFilter(GDALDataset* sourceDataset,
	              const std::string& filterPath,
	              const std::string& targetPath,
	              ProgressType progress = nullptr)
		: Transformation({sourceDataset}, targetPath, progress),
		  _sievePath(filterPath), _sieveDataset(nullptr)
	{ }

	ClusterFilter(const ClusterFilter&) = delete;
	ClusterFilter& operator=(const ClusterFilter&) = delete;
	~ClusterFilter();

	/// <summary>
	/// Retrieves the sieve filter dataset.
	/// </summary>
	/// <remarks>
	/// By calling this method, the sieve filter datatset will be released by the transformation and won't be automatically freed.
	/// </remarks>
	/// <returns>The sieve filter dataset.</returns>
	GDALDataset* filter();

protected:
	/// <summary>
	/// Verifies source and calculates the metadata for the target.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Produces the target.
	/// </summary>
	void onExecute() override;

private:
	/// <summary>
	/// Routes the C-style GDAL progress reports to the defined reporter.
	/// </summary>
	static int CPL_STDCALL gdalProgress(double dfComplete, const char* pszMessage, void* pProgressArg);
};
} // Buildings
} // AHN
