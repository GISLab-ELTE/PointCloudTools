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

	CloudTools::DEM::ClusterMap clusterMap;

	int threshold = -1;

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="mode">The applied morphology method.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MorphologyClusterFilter(CloudTools::DEM::ClusterMap& source,
		const std::string& targetPath,
		Method method = Method::Dilation,
		Operation::ProgressType progress = nullptr)
		: clusterMap(source), method(method)
	{

	}

	MorphologyClusterFilter(){}

	MorphologyClusterFilter(const MorphologyClusterFilter&) = delete;
	MorphologyClusterFilter& operator=(const MorphologyClusterFilter&) = delete;

	void onPrepare(){}

	void onExecute()
	{
		if (this->method == Method::Dilation && this->threshold == -1)
			this->threshold = 0;
		if (this->method == Method::Erosion && this->threshold == -1)
			this->threshold = 9;

		int counter;
		for (GUInt32 index : clusterMap.clusterIndexes())
			for (const OGRPoint& p : clusterMap.points(index))
			{
				counter = 0;
				for (int i = p.getX() - 1; i <= p.getX() + 1; i++)
					for (int j = p.getY() - 1; j <= p.getY() + 1; j++)
						if (clusterMap.clusterIndex(i, j) == clusterMap.clusterIndex(p.getX(), p.getY()))
							++counter;

				if (this->method == Method::Dilation && counter > this->threshold)
					this->targetMap.addPoint(index, p.getX(), p.getY());
				if (this->method == Method::Erosion && counter < this->threshold)
				{ // TODO}

				}
			}
	}

	CloudTools::DEM::ClusterMap& target()
	{
		return this->targetMap;
	}

private:
	CloudTools::DEM::ClusterMap targetMap;
};
} //Vegetation
} //AHN