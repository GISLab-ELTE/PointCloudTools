#pragma once

#include <string>

#include "CloudTools.Common/Operation.h"
#include "CloudTools.DEM/Window.hpp"
#include "CloudTools.DEM/ClusterMap.h"

namespace AHN
{
namespace Vegetation
{
class MorphologyClusterFilter : public CloudTools::Operation
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

	int threshold = -1;

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="mode">The applied morphology method.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MorphologyClusterFilter(const CloudTools::DEM::ClusterMap& sourcePath,
		const std::string& targetPath,
		Method method = Method::Dilation,
		Operation::ProgressType progress = nullptr)
		: method(method)
	{

	}

	MorphologyClusterFilter(){}

	MorphologyClusterFilter(const MorphologyClusterFilter&) = delete;
	MorphologyClusterFilter& operator=(const MorphologyClusterFilter&) = delete;

	void onPrepare(){}

	void onExecute(){}
};
}
}