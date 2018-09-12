#include <algorithm>
#include <iostream>
#include "ClusterMap.h"

namespace CloudTools
{
namespace DEM
{
GUInt32 ClusterMap::clusterIndex(int x, int y) const
{
	return _clusterPoints.at(std::make_pair(x, y));
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

void ClusterMap::addPoint(GUInt32 clusterIndex, int x, int y)
{
	Point point = std::make_pair(x, y);

	if (_clusterIndexes.find(clusterIndex) == _clusterIndexes.end())
		throw std::out_of_range("Cluster is out of range.");

	if (std::find(_clusterIndexes[clusterIndex].begin(),
		_clusterIndexes[clusterIndex].end(), point)
		!= _clusterIndexes[clusterIndex].end())
		throw std::logic_error("Point is already in cluster.");

	_clusterIndexes[clusterIndex].push_back(point);
	_clusterPoints.insert(std::make_pair(point, clusterIndex));
}

std::unordered_set<Point> ClusterMap::getNeighbors(GUInt32 clusterIndex)
{
  std::unordered_set<Point> neighbors;
  Point point;
  for (const Point &p : points(clusterIndex))
  {
    for (int i = p.first - 1; i <= p.first + 1; i++)
      for (int j = p.second - 1; j <= p.second + 1; j++)
        if (i != p.first || j != p.second)
        {
          point = std::make_pair(i, j);
          auto it = _clusterPoints.find(point);
          if (it == _clusterPoints.end())
          {
            neighbors.insert(point);
          }
        }
  }

  return neighbors;
}

const std::vector<Point>& ClusterMap::points(GUInt32 clusterIndex) const
{
	return _clusterIndexes.at(clusterIndex);
}

void ClusterMap::createCluster(int x, int y)
{
	Point point = std::make_pair(x, y);
	if (_clusterPoints.find(point) != _clusterPoints.end())
		throw std::logic_error("Point already in cluster map.");

	_clusterIndexes[_nextClusterIndex].push_back(point);
	_clusterPoints[point] = _nextClusterIndex;
	++_nextClusterIndex;
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
	if(_clusterIndexes[clusterB].size() > _clusterIndexes[clusterA].size())
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
}

void ClusterMap::removeCluster(GUInt32 clusterIndex)
{
	if (_clusterIndexes.find(clusterIndex) == _clusterIndexes.end())
		throw std::out_of_range("The specified cluster does not exist.");

	for (const auto& point : _clusterIndexes[clusterIndex])
		_clusterPoints.erase(point);
	_clusterIndexes.erase(clusterIndex);
}

std::size_t ClusterMap::removeSmallClusters(int threshold)
{
	std::vector<GUInt32> removedIndexes;
	for (const auto& item : _clusterIndexes)
		if(item.second.size() < threshold)
		{
			removedIndexes.push_back(item.first);
		}
	
	for(int index : removedIndexes)
	{
		removeCluster(index);
	}
	return removedIndexes.size();
}
} // DEM
} // CloudTools
