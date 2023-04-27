#include <numeric>

#include "VolumeDifference.h"

namespace CloudTools
{
namespace Vegetation
{
void VolumeDifference::calculateVolume()
{
	std::pair<double, std::map<GUInt32, double>> volumeA = calculateLonelyEpochVolume(Epoch::A, this->clusterMapA);
	this->fullVolumeA = volumeA.first;
	this->lonelyVolumeA = volumeA.second;

	std::pair<double, std::map<GUInt32, double>> volumeB = calculateLonelyEpochVolume(Epoch::B, this->clusterMapB);
	this->fullVolumeB = volumeB.first;
	this->lonelyVolumeB = volumeB.second;

	calculateDifference();
}

std::pair<double, std::map<GUInt32, double>> VolumeDifference::calculateLonelyEpochVolume(Epoch epoch, ClusterMap& map)
{
	double fullVolume = 0.0;
	std::map<GUInt32, double> lonelyVolume;
	std::vector<GUInt32> lonely;

	if (epoch == Epoch::A)
		lonely = this->distance->lonelyA();

	if (epoch == Epoch::B)
		lonely = this->distance->lonelyB();

	for (const auto& elem : lonely)
	{
		double volume = std::accumulate(map.points(elem).begin(), map.points(elem).end(),
		                                0.0, [](double sum, const OGRPoint& point)
		                                {
			                                return sum + point.getZ();
		                                });
		volume *= 0.25;
		lonelyVolume.insert(std::make_pair(elem, volume));
		fullVolume += std::abs(volume);
	}

	return std::make_pair(fullVolume, lonelyVolume);
}

void VolumeDifference::calculateDifference()
{
	double clusterVolumeA, clusterVolumeB;

	for (const auto& elem : this->distance->closest())
	{
		clusterVolumeA = std::accumulate(this->clusterMapA.points(elem.first.first).begin(),
		                                 this->clusterMapA.points(elem.first.first).end(), 0.0,
		                                 [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		clusterVolumeA *= 0.25;
		this->fullVolumeA += std::abs(clusterVolumeA);

		clusterVolumeB = std::accumulate(this->clusterMapB.points(elem.first.second).begin(),
		                                 this->clusterMapB.points(elem.first.second).end(), 0.0,
		                                 [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		clusterVolumeB *= 0.25;
		this->fullVolumeB += std::abs(clusterVolumeB);

		this->diffs.insert(std::make_pair(elem.first, clusterVolumeB - clusterVolumeA));
	}
}
} // Vegetation
} // CloudTools
