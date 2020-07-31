#include <numeric>
#include "VolumeDifference.h"

namespace AHN
{
namespace Vegetation
{
void VolumeDifference::calculateVolume()
{
	std::pair<double, std::map<GUInt32, double>> ahn2_volume = calculateLonelyEpochVolume(AHN2, this->ahn2);
	this->ahn2_fullVolume = ahn2_volume.first;
	this->ahn2_lonelyVolume = ahn2_volume.second;

	std::pair<double, std::map<GUInt32, double>> ahn3_volume = calculateLonelyEpochVolume(AHN3, this->ahn3);
	this->ahn3_fullVolume = ahn3_volume.first;
	this->ahn3_lonelyVolume = ahn3_volume.second;

	calculateDifference();
}

std::pair<double, std::map<GUInt32, double>> VolumeDifference::calculateLonelyEpochVolume(AHN epoch, ClusterMap& map)
{
	double fullVolume = 0.0;
	std::map<GUInt32, double> lonelyVolume;
	std::vector<GUInt32> lonely;

	if (epoch == AHN2)
		lonely = this->distance->lonelyAHN2();

	if (epoch == AHN3)
		lonely = this->distance->lonelyAHN3();

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
	double ahn2ClusterVolume, ahn3ClusterVolume;

	for (const auto& elem : this->distance->closest())
	{
		ahn2ClusterVolume = std::accumulate(this->ahn2.points(elem.first.first).begin(),
		                                    this->ahn2.points(elem.first.first).end(), 0.0,
		                                    [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		ahn2ClusterVolume *= 0.25;
		this->ahn2_fullVolume += std::abs(ahn2ClusterVolume);

		ahn3ClusterVolume = std::accumulate(this->ahn3.points(elem.first.second).begin(),
		                                    this->ahn3.points(elem.first.second).end(), 0.0,
		                                    [](double sum, const OGRPoint& point)
		                                    {
			                                    return sum + point.getZ();
		                                    });
		ahn3ClusterVolume *= 0.25;
		this->ahn3_fullVolume += std::abs(ahn3ClusterVolume);

		this->diffs.insert(std::make_pair(elem.first, ahn3ClusterVolume - ahn2ClusterVolume));
	}
}
} // Vegetation
} // AHN
