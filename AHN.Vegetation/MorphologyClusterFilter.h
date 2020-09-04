#pragma once

#include <CloudTools.Common/Operation.h>
#include <CloudTools.DEM/ClusterMap.h>
#include <CloudTools.DEM/DatasetCalculation.hpp>

namespace AHN
{
namespace Vegetation
{
class MorphologyClusterFilter : public CloudTools::DEM::DatasetCalculation<float>
{
public:
	enum Method
	{
		Dilation,
		Erosion
	};

	/// <summary>
	/// The applied morphology method.
	/// </summary>
	Method method;

	/// <summary>
	/// Threshold value for morphology filter.
	///
	/// Default value is -1, which will be 0 for dilation and 9 for erosion.
	/// </summary>
	int threshold = -1;

private:
	CloudTools::DEM::ClusterMap _clusterMap;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="source">The cluster map to apply the morphological filter on.</param>
	/// <param name="sourceDatasets">The source raster datasets storing the height values for the points.</param>
	/// <param name="method">The method (DILATION or EROSION) to apply.</param>
	/// <param name="progress">The callback method to report progress.</param>
	/// <returns></returns>
	MorphologyClusterFilter(CloudTools::DEM::ClusterMap& source,
	                        const std::vector<GDALDataset*>& sourceDatasets,
	                        Method method = Method::Dilation,
	                        ProgressType progress = nullptr)
		: DatasetCalculation(sourceDatasets, nullptr, progress),
		  _clusterMap(source), method(method)
	{
		initialize();
	}

	MorphologyClusterFilter(const MorphologyClusterFilter&) = delete;

	MorphologyClusterFilter& operator=(const MorphologyClusterFilter&) = delete;

	CloudTools::DEM::ClusterMap& target();

private:
	void initialize();
};
} // Vegetation
} // AHN