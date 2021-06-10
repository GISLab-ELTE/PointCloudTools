#include <algorithm>
#include <numeric>
#include <unordered_set>
#include <stdexcept>

#include "ClusterMap.h"

namespace CloudTools
{
namespace DEM
{
void ClusterMap::setSizeX(int x)
{
	_sizeX = x;
}

void ClusterMap::setSizeY(int y)
{
	_sizeY = y;
}

int ClusterMap::sizeX()
{
	return _sizeX;
}

int ClusterMap::sizeY()
{
	return _sizeY;
}

GUInt32 ClusterMap::clusterIndex(int x, int y) const
{
	return _clusterPoints.at(OGRPoint(x, y));
}

std::vector<GUInt32> ClusterMap::clusterIndexes() const
{
	std::vector<GUInt32> indexes;
	indexes.reserve(_clusterIndexes.size());

	for (const auto& item : _clusterIndexes)
	{
		indexes.push_back(item.first);
	}
	return indexes;
}

void ClusterMap::addPoint(GUInt32 clusterIndex, int x, int y, double z)
{
	OGRPoint point(x, y, z);

	if (_clusterIndexes.find(clusterIndex) == _clusterIndexes.end())
		throw std::out_of_range("Cluster is out of range.");

	if (std::find_if(_clusterIndexes[clusterIndex].begin(),
	                 _clusterIndexes[clusterIndex].end(),
	                 [&point](OGRPoint& p)
	                 {
		                 return point.getX() == p.getX() &&
			                 point.getY() == p.getY();
	                 })
		!= _clusterIndexes[clusterIndex].end())
		throw std::logic_error("Point is already in cluster.");

	_clusterIndexes[clusterIndex].push_back(point);
	_clusterPoints.insert(std::make_pair(point, clusterIndex));
}

void ClusterMap::removePoint(GUInt32 clusterIndex, int x, int y)
{
	if (_clusterIndexes.find(clusterIndex) == _clusterIndexes.end())
		throw std::out_of_range("Cluster is out of range.");

	OGRPoint point(x, y);

	std::vector<OGRPoint>::iterator iter =
		std::find_if(_clusterIndexes[clusterIndex].begin(),
		             _clusterIndexes[clusterIndex].end(),
		             [&point](OGRPoint& p)
		             {
			             return point.getX() == p.getX() &&
				             point.getY() == p.getY();
		             });

	if (iter == _clusterIndexes[clusterIndex].end())
		throw std::out_of_range("Point is out of range.");

	_clusterIndexes[clusterIndex].erase(iter);
	_clusterPoints.erase(point);

	if (_clusterIndexes[clusterIndex].empty())
		removeCluster(clusterIndex);
	else if (_seedPoints[clusterIndex].getX() == point.getX() &&
		_seedPoints[clusterIndex].getY() == point.getY())
		_seedPoints.erase(clusterIndex);
}

std::vector<OGRPoint> ClusterMap::neighbors(GUInt32 clusterIndex) const
{
	std::unordered_set<OGRPoint, PointHash, PointEqual> neighbors;
	for (const OGRPoint& p : points(clusterIndex))
	{
		for (int i = p.getX() - 1; i <= p.getX() + 1; i++)
			for (int j = p.getY() - 1; j <= p.getY() + 1; j++)
				if (i >= 0 && i < _sizeX &&
					j >= 0 && j < _sizeY &&
					(i != p.getX() || j != p.getY()))
				{
					OGRPoint point(i, j, p.getZ());
					if (_clusterPoints.find(point) == _clusterPoints.end())
						neighbors.insert(point);
				}
	}

	return std::vector<OGRPoint>(neighbors.begin(), neighbors.end());
}

OGRPoint ClusterMap::center3D(GUInt32 clusterIndex) const
{
	auto clusterPoints = points(clusterIndex);
	int avgX = std::accumulate(clusterPoints.begin(), clusterPoints.end(), 0,
	                           [](int value, OGRPoint& p) { return value + p.getX(); }) / clusterPoints.size();
	int avgY = std::accumulate(clusterPoints.begin(), clusterPoints.end(), 0,
	                           [](int value, OGRPoint& p) { return value + p.getY(); }) / clusterPoints.size();
	double avgZ = std::accumulate(clusterPoints.begin(), clusterPoints.end(), 0,
	                              [](double value, OGRPoint& p) { return value + p.getZ(); }) / clusterPoints.size();
	return OGRPoint(avgX, avgY, avgZ);
}

OGRPoint ClusterMap::center2D(GUInt32 clusterIndex) const
{
	auto clusterPoints = points(clusterIndex);
	int avgX = std::accumulate(clusterPoints.begin(), clusterPoints.end(), 0,
	                           [](int value, OGRPoint& p) { return value + p.getX(); }) / clusterPoints.size();
	int avgY = std::accumulate(clusterPoints.begin(), clusterPoints.end(), 0,
	                           [](int value, OGRPoint& p) { return value + p.getY(); }) / clusterPoints.size();

	return OGRPoint(avgX, avgY);
}

OGRPoint ClusterMap::highestPoint(GUInt32 clusterIndex) const
{
	auto clusterPoints = points(clusterIndex);
	OGRPoint highest = clusterPoints.at(0);
	for (const auto& p : clusterPoints)
		if (p.getZ() > highest.getZ())
			highest = p;

	return highest;
}

OGRPoint ClusterMap::lowestPoint(GUInt32 clusterIndex) const
{
	auto clusterPoints = points(clusterIndex);
	OGRPoint lowest = clusterPoints.at(0);
	for (const auto& p : clusterPoints)
		if (p.getZ() < lowest.getZ())
			lowest = p;

	return lowest;
}

std::vector<OGRPoint> ClusterMap::boundingBox(GUInt32 clusterIndex) const
{
	std::vector<OGRPoint> borders;

	auto clusterPoints = points(clusterIndex);
	OGRPoint upperRight = clusterPoints.at(0);
	for (const auto& p : clusterPoints)
		if (p.getX() > upperRight.getX() && p.getY() > upperRight.getY())
			upperRight = p;

	borders.push_back(upperRight);

	OGRPoint lowerRight = clusterPoints.at(0);
	for (const auto& p : clusterPoints)
		if (p.getX() < lowerRight.getX() && p.getY() > lowerRight.getY())
			lowerRight = p;

	borders.push_back(lowerRight);

	OGRPoint lowerLeft = clusterPoints.at(0);
	for (const auto& p : clusterPoints)
		if (p.getX() < lowerLeft.getX() && p.getY() < lowerLeft.getY())
			lowerLeft = p;

	borders.push_back(lowerLeft);

	OGRPoint upperLeft = clusterPoints.at(0);
	for (const auto& p : clusterPoints)
		if (p.getX() > upperLeft.getX() && p.getY() < upperLeft.getY())
			upperLeft = p;

	borders.push_back(upperLeft);

	return borders;
}

OGRPoint ClusterMap::seedPoint(GUInt32 clusterIndex) const
{
	return _seedPoints.at(clusterIndex);
}

const std::vector<OGRPoint>& ClusterMap::points(GUInt32 clusterIndex) const
{
	return _clusterIndexes.at(clusterIndex);
}

GUInt32 ClusterMap::createCluster(int x, int y, double z)
{
	OGRPoint point(x, y, z);

	if (_clusterPoints.find(point) != _clusterPoints.end())
		throw std::logic_error("Point already in cluster map.");

	_clusterIndexes[_nextClusterIndex].push_back(point);
	_clusterPoints[point] = _nextClusterIndex;
	_seedPoints[_nextClusterIndex] = point;
	return _nextClusterIndex++;
}

void ClusterMap::mergeClusters(GUInt32 clusterA, GUInt32 clusterB)
{
	if (_clusterIndexes.find(clusterA) == _clusterIndexes.end())
		throw std::out_of_range("The parameter cluster A is out of range.");
	if (_clusterIndexes.find(clusterB) == _clusterIndexes.end())
		throw std::out_of_range("The parameter cluster B is out of range.");
	if (clusterA == clusterB)
		return;

	// Merge the smaller cluster into the larger
	GUInt32 fromCluster = clusterB;
	GUInt32 toCluster = clusterA;
	if (_clusterIndexes[clusterB].size() > _clusterIndexes[clusterA].size())
	{
		fromCluster = clusterA;
		toCluster = clusterB;
	}

	// Update point to cluster map
	for (const auto& point : _clusterIndexes[fromCluster])
		_clusterPoints[point] = toCluster;

	// Update cluster to points map
	_clusterIndexes[toCluster].insert(
		_clusterIndexes[toCluster].end(),
		std::make_move_iterator(_clusterIndexes[fromCluster].begin()),
		std::make_move_iterator(_clusterIndexes[fromCluster].end()));

	// Remove merged cluster
	_clusterIndexes.erase(fromCluster);
	_seedPoints.erase(fromCluster);
}

void ClusterMap::removeCluster(GUInt32 clusterIndex)
{
	if (_clusterIndexes.find(clusterIndex) == _clusterIndexes.end())
		throw std::out_of_range("The specified cluster does not exist.");

	for (const auto& point : _clusterIndexes[clusterIndex])
		_clusterPoints.erase(point);
	_clusterIndexes.erase(clusterIndex);
	_seedPoints.erase(clusterIndex);
}

std::size_t ClusterMap::removeSmallClusters(unsigned int threshold)
{
	std::vector<GUInt32> removedIndexes;
	for (const auto& item : _clusterIndexes)
		if (item.second.size() < threshold)
		{
			removedIndexes.push_back(item.first);
		}

	for (int index : removedIndexes)
	{
		removeCluster(index);
	}
	return removedIndexes.size();
}

void ClusterMap::shuffle()
{

	for (auto& item : _clusterIndexes)
	{
		std::shuffle(item.second.begin(), item.second.end(), engine);
	}
}

std::random_device ClusterMap::rd;
std::mt19937 ClusterMap::engine = std::mt19937(ClusterMap::rd());
} // DEM
} // CloudTools
