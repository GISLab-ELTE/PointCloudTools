#include <algorithm>

#include "MorphologyClusterFilter.h"

namespace CloudTools
{
namespace Vegetation
{
void MorphologyClusterFilter::initialize()
{
	this->computation = [this](int sizeX, int sizeY)
	{
		if (this->method == Method::Dilation && this->threshold == -1)
			this->threshold = 0;
		if (this->method == Method::Erosion && this->threshold == -1)
			this->threshold = 9;

		int counter;
		if (this->method == Method::Erosion)
		{
			std::vector<OGRPoint> pointSet;
			for (GUInt32 index : _clusterMap.clusterIndexes())
			{
				pointSet.clear();
				for (const OGRPoint& p : _clusterMap.points(index))
				{
					counter = 0;
					for (int i = p.getX() - 1; i <= p.getX() + 1; i++)
						for (int j = p.getY() - 1; j <= p.getY() + 1; j++)
						{
							OGRPoint point(i, j);
							if (std::find_if(_clusterMap.points(index).begin(),
							                 _clusterMap.points(index).end(),
							                 [&point](const OGRPoint& p)
							                 {
								                 return point.getX() == p.getX() &&
								                        point.getY() == p.getY();
							                 })
							    != _clusterMap.points(index).end()
							    && _clusterMap.clusterIndex(i, j) == _clusterMap.clusterIndex(p.getX(), p.getY()))
							{
								++counter;
							}
						}
					if (counter < this->threshold)
						pointSet.emplace_back(p);
				}

				for (auto p : pointSet)
					_clusterMap.removePoint(index, p.getX(), p.getY());
			}
		}

		if (this->method == Method::Dilation)
		{
			std::vector<OGRPoint> pointSet;
			for (GUInt32 index : _clusterMap.clusterIndexes())
			{
				pointSet.clear();
				for (OGRPoint& p : _clusterMap.neighbors(index))
				{
					if (!hasSourceData(p.getX(), p.getY()))
						continue;

					counter = 0;
					for (int i = p.getX() - 1; i <= p.getX() + 1; i++)
						for (int j = p.getY() - 1; j <= p.getY() + 1; j++)
						{
							OGRPoint point(i, j);
							if (std::find_if(_clusterMap.points(index).begin(),
							                 _clusterMap.points(index).end(),
							                 [&point](const OGRPoint& p)
							                 {
								                 return point.getX() == p.getX() &&
								                        point.getY() == p.getY();
							                 })
							    != _clusterMap.points(index).end()
							    && _clusterMap.clusterIndex(i, j) == index)
								++counter;
						}
					if (counter > this->threshold)
						pointSet.emplace_back(p);
				}
				for (auto& p : pointSet)
					_clusterMap.addPoint(index, p.getX(), p.getY(), sourceData(p.getX(), p.getY()));
			}
		}
	};
}

CloudTools::DEM::ClusterMap& MorphologyClusterFilter::target()
{
	return this->_clusterMap;
}
} // Vegetation
} // CloudTools
