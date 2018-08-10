#pragma once

#include <string>

#include "../DatasetTransformation.hpp"
#include "../ClusterMap.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a hierarchical clustering for DEM datasets.
/// </summary>
template <typename DataType = float>
class HierarchicalClustering : public DatasetTransformation<GUInt32, DataType>
{
public:
	enum Method
	{
		Agglomerative,
		//Divisive
	};
	
	/// <summary>
	/// The applied hierarchical clustering method.
	/// </summary>
	Method method;

	/// <summary>
	/// The threshold of accepted height difference between neighbouring grid points.
	/// </summary>
	double threshold = 0.5;

	/// <summary>
	/// The maximum number of iterations to be applied in the algorithm.
	/// </summary>
	int maxIterations = 100;

	/// <summary>
	/// The minimum size of clusters to include in the final result.
	/// </summary>
	int minimumSize = 4;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the algorithm.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="mode">The applied hierarchical clustering method.</param>
	/// <param name="progress">The callback method to report progress.</param>
	HierarchicalClustering(const std::string& sourcePath,
	                       const std::string& targetPath,
	                       Method method = Method::Agglomerative,
					       Operation::ProgressType progress = nullptr)
		: DatasetTransformation<GUInt32, DataType>({ sourcePath }, targetPath, nullptr, progress),
		  method(method)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the algorithm.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="mode">The applied hierarchical clustering method.</param>
	/// <param name="progress">The callback method to report progress.</param>
	HierarchicalClustering(GDALDataset* sourceDataset,
		                   const std::string& targetPath,
		                   Method method = Method::Agglomerative,
					       Operation::ProgressType progress = nullptr)
		: DatasetTransformation<GUInt32, DataType>({ sourceDataset }, targetPath, nullptr, progress),
		  method(method)
	{
		initialize();
	}

	HierarchicalClustering(const HierarchicalClustering&) = delete;
	HierarchicalClustering& operator=(const HierarchicalClustering&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};

template <typename DataType>
void HierarchicalClustering<DataType>::initialize()
{
	this->nodataValue = 0;

	// https://en.wikipedia.org/wiki/Hierarchical_clustering
	this->computation = [this](int sizeX, int sizeY)
	{
		if (this->maxIterations < 1)
			this->maxIterations = 1;

		ClusterMap clusterMap;

		switch (method)
		{
		case Method::Agglomerative:
		{
			for (int i = 0; i < sizeX; ++i)
				for (int j = 0; j < sizeY; ++j)
					if (this->hasSourceData(i, j))
					{
						clusterMap.createCluster(i, j);
					}

			auto mergeClusters = [this, &clusterMap](int x1, int y1, int x2, int y2)
			{
				if (this->hasSourceData(x1, y1) && this->hasSourceData(x2, y2) &&
					clusterMap.clusterIndex(x1, y1) != clusterMap.clusterIndex(x2, y2))
				{
					DataType diff = std::abs(this->sourceData(x1, y1) - this->sourceData(x2, y2));
					if (diff < this->threshold)
					{
						clusterMap.mergeClusters(
							clusterMap.clusterIndex(x1, y1),
							clusterMap.clusterIndex(x2, y2));

						return true;
					}
				}
				return false;
			};
			if (this->progress)
				this->progress(0.1f, "Cluster map created");

			int changes;
			int iteration = 0;
			do
			{
				changes = 0;
				++iteration;

				for (int i = 0; i < sizeX; ++i)
					for (int j = 0; j < sizeY; ++j)
						if (this->hasSourceData(i, j))
						{
							/*
							 * Right & downwards merge direction:
							 * o 1
							 * 2 3
							 */

							bool merged = false;
							merged |= mergeClusters(i, j, i, j + 1);
							merged |= mergeClusters(i, j, i + 1, j);
							merged |= mergeClusters(i, j, i + 1, j + 1);

							if (merged)
								++changes;
						}

				if (this->progress)
					this->progress(0.1f + 0.8f / this->maxIterations,
						"Finished clustering round #" + std::to_string(iteration) +
						" with " + std::to_string(changes) + " changes");
			} while (changes > 0 && iteration < this->maxIterations);

			break;
		}
		}
		if (this->progress)
			this->progress(0.9f, "Clustering completed");

		if (this->minimumSize > 1)
		{
			clusterMap.removeSmallClusters(this->minimumSize);
			if (this->progress)
				this->progress(0.95f, "Small clusters removed");
		}

		for (GUInt32 index : clusterMap.clusterIndexes())
			for (const ClusterMap::Point& point : clusterMap.points(index))
			{
				this->setTargetData(point.first, point.second, index);
			}

		if (this->progress)
			this->progress(1.f, "Target created");
	};
}
} // DEM
} // CloudTools
