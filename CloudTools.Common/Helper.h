#pragma once

#include <ogr_geometry.h>

#include <boost/functional/hash/hash.hpp>

namespace CloudTools
{
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

/// <summary>
/// Represents a (X, Y) point equality functor for OGRPoint.
/// </summary>
struct PointEqual
{
	bool operator()(const OGRPoint& a, const OGRPoint& b) const
	{
		return a.getX() == b.getX() && a.getY() == b.getY();
	}
};

/// <summary>
/// Represents a (X, Y) point comparator for OGRPoint.
/// </summary>
struct PointComparator
{
	bool operator()(const OGRPoint& a, const OGRPoint& b) const
	{
		if (a.getX() < b.getX())
			return true;
		else if (a.getX() > b.getX())
			return false;
		else
			return a.getY() < b.getY();
	}
};
} // CloudTools
