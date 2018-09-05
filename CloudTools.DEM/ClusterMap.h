#pragma once

#include <vector>
#include <unordered_map>

#include <gdal.h>

#include "Helper.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a cluster map of a DEM dataset.
/// </summary>
class ClusterMap
{
public:
	/// <summary>
	/// nitializes a new, empty instance of the class.
	/// </summary>
	ClusterMap() = default;

	/// <summary>
	/// Retrieves the cluster index for a given grid point.
	/// </summary>
	/// <param name="x">The abcissa of the point.</param>
	/// <param name="y">The ordinate of the point.</param>
	/// <returns>The cluster index for the point.</returns>
	GUInt32 clusterIndex(int x, int y) const;

	/// <summary>
	/// Retrieves all cluster indexes.
	/// </summary>
	/// <returns>The indexes.</returns>
	std::vector<GUInt32> clusterIndexes() const;

	/// <summary>
	/// Attaches a given grid point to the given cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <param name="x">The abcissa of the point.</param>
	/// <param name="y">The ordinate of the point.</param>
	void addPoint(GUInt32 clusterIndex, int x, int y);

	std::vector<Point> getNeighbors(GUInt32 clusterIndex, int x, int y);

	/// <summary>
	/// Retrieves the points in a cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The points contained by the cluster.</returns>
	const std::vector<Point>& points(GUInt32 clusterIndex) const;

	/// <summary>
	/// Creates a new cluster with an initial point.
	/// </summary>
	/// <param name="x">The abcissa of the initial point.</param>
	/// <param name="y">The ordinate of the inital point.</param>
	void createCluster(int x, int y);

	/// <summary>
	/// Merges 2 clusters.
	/// </summary>
	/// <remarks>
	/// The smaller cluster will be merged into the larger one.
	/// </remarks>
	/// <param name="clusterA"></param>
	/// <param name="clusterB"></param>
	void mergeClusters(GUInt32 clusterA, GUInt32 clusterB);

	/// <summary>
	/// Removes a cluster from the mapping.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster to be removed.</param>
	void removeCluster(GUInt32 clusterIndex);

	/// <summary>
	/// Removes small clusters.
	/// </summary>
	/// <param name="threshold">The minimum threshold of size to keep a cluster.</param>
	/// <returns>The number of removed clusters.</returns>
	std::size_t removeSmallClusters(int threshold);

private:
	std::unordered_map<GUInt32, std::vector<Point>> _clusterIndexes;
	std::unordered_map<Point, GUInt32> _clusterPoints;
	GUInt32 _nextClusterIndex = 1;
};
} // DEM
} // CloudTools
