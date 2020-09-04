#pragma once

#include <vector>
#include <unordered_map>

#include <boost/functional/hash/hash.hpp>

#include <gdal.h>
#include <ogr_geometry.h>

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
private:
	/// <summary>
	/// Represents a (X, Y) point hashing functor for OGRPoint.
	/// </summary>
	struct PointHash
	{
		std::size_t operator()(const OGRPoint& p) const
		{
			std::size_t seed = 0;
			auto h1 = std::hash<double>{}(p.getX());
			auto h2 = std::hash<double>{}(p.getY());

			boost::hash_combine(seed, h1);
			boost::hash_combine(seed, h2);
			return seed;
		}
	};

	struct PointEqual
	{
		bool operator()(const OGRPoint& a, const OGRPoint& b) const
		{
			return a.getX() == b.getX() && a.getY() == b.getY();
		}
	};

private:
	std::unordered_map<GUInt32, std::vector<OGRPoint>> _clusterIndexes;
	std::unordered_map<OGRPoint, GUInt32, PointHash, PointEqual> _clusterPoints;
	GUInt32 _nextClusterIndex = 1;
	std::unordered_map<GUInt32, OGRPoint> _seedPoints;
	int _sizeX, _sizeY;

public:
	/// <summary>
	/// Initializes a new, empty instance of the class.
	/// </summary>
	ClusterMap() = default;

	/// <summary>
	/// Initializes a new instance of the class with maximum width and height.
	/// </summary>
	/// <param name="sizeX">The height of the cluster map.</param>
	/// <param name="sizeY">The width of the cluster map.</param>
	ClusterMap(int sizeX, int sizeY) : _sizeX(sizeX), _sizeY(sizeY)
	{
	}

	void setSizeX(int x);

	void setSizeY(int y);

	int sizeX();

	int sizeY();

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
	/// <param name="z">The height of the initial point.</param>
	/// </summary>
	void addPoint(GUInt32 clusterIndex, int x, int y, double z = 0.0);

	/// <summary>
	/// Eliminates a given grid point from the given cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <param name="x">The abcissa of the point.</param>
	/// <param name="y">The ordinate of the point.</param>
	void removePoint(GUInt32 clusterIndex, int x, int y);

	/// <summary>
	/// Retrieves the direct neighbors of the points in a cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The neighboring points contained by the cluster.</returns>
	std::vector<OGRPoint> neighbors(GUInt32 clusterIndex) const;

	/// <summary>
	/// Calculates the center of gravity (?) of a cluster by
	/// taking the average of the coordinates of its points.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The center of gravity of the cluster.</returns>
	OGRPoint center(GUInt32 clusterIndex) const;

	/// <summary>
	/// Retrieves the point with the biggest Z coordinate in the cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The highest point of the cluster.</returns>
	OGRPoint highestPoint(GUInt32 clusterIndex) const;

	/// <summary>
	/// Retrieves the point with the lowest Z coordinate in the cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The lowest point of the cluster.</returns>
	OGRPoint lowestPoint(GUInt32 clusterIndex) const;

	/// <summary>
	/// Returns the minimal bounding box of the cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns><The minimal bounding box of the cluster./returns>
	std::vector<OGRPoint> boundingBox(GUInt32 clusterIndex) const;

	/// <summary>
	/// Retrieves the seed point of a cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The seed point of the cluster.</returns>
	OGRPoint seedPoint(GUInt32 clusterIndex) const;

	/// <summary>
	/// Retrieves the points in a cluster.
	/// </summary>
	/// <param name="clusterIndex">The index of the cluster.</param>
	/// <returns>The points contained by the cluster.</returns>
	const std::vector<OGRPoint>& points(GUInt32 clusterIndex) const;

	/// <summary>
	/// Creates a new cluster with an initial point.
	/// </summary>
	/// <param name="x">The abcissa of the initial point.</param>
	/// <param name="y">The ordinate of the initial point.</param>
	/// <param name="z">The height of the initial point.</param>
	/// <returns>The id of the created cluster.</returns>
	GUInt32 createCluster(int x, int y, double z = 0.0);

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
	std::size_t removeSmallClusters(unsigned int threshold);
};
} // DEM
} // CloudTools
