#ifndef CLOUDTOOLS_VOLUMEDIFFERENCE_H
#define CLOUDTOOLS_VOLUMEDIFFERENCE_H

#include <map>

#include <CloudTools.DEM/ClusterMap.h>

using namespace CloudTools::DEM;

template <typename T>
class VolumeDifference
{
public:
  enum AHN {AHN2, AHN3};

  ClusterMap ahn2, ahn3;
  T* distance;

  double ahn2_fullVolume, ahn3_fullVolume;
  std::map<GUInt32, double> ahn2_lonelyVolume, ahn3_lonelyVolume;
  std::map<std::pair<GUInt32, GUInt32>, double> diffs;

  VolumeDifference(ClusterMap& ahn2_map,
                  ClusterMap& ahn3_map,
                  T* distance)
                  : ahn2(ahn2_map), ahn3(ahn3_map), distance(distance)
  {
    calculateVolume();
  }

private:
  void calculateVolume();

  std::pair<double, std::map<GUInt32, double>> calculateLonelyEpochVolume(AHN epoch, ClusterMap& map);

  void calculateDifference();
};

template <typename T>
void VolumeDifference<T>::calculateVolume()
{
  std::pair<double, std::map<GUInt32, double>> ahn2_volume = calculateLonelyEpochVolume(AHN2, this->ahn2);
  this->ahn2_fullVolume = ahn2_volume.first;
  this->ahn2_lonelyVolume = ahn2_volume.second;

  std::pair<double, std::map<GUInt32, double>> ahn3_volume = calculateLonelyEpochVolume(AHN3, this->ahn3);
  this->ahn3_fullVolume = ahn3_volume.first;
  this->ahn3_lonelyVolume = ahn3_volume.second;

  calculateDifference();
}

template <typename T>
std::pair<double, std::map<GUInt32, double>> VolumeDifference<T>::calculateLonelyEpochVolume(AHN epoch, ClusterMap& map)
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

template <typename T>
void VolumeDifference<T>::calculateDifference()
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

#endif //CLOUDTOOLS_VOLUMEDIFFERENCE_H
