#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <CloudTools.DEM/SweepLineCalculation.hpp>
#include <CloudTools.DEM/DatasetCalculation.hpp>


namespace CloudTools::DEM
{
/// <summary>
/// Represents the removal of false positive seed points close to buildings
/// </summary>
template <typename DataType = float>
class BuildingFacadeSeedRemoval : public DatasetCalculation<DataType>
{
	std::vector<OGRPoint> &seedPoints;
public:
	int threshold;
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourcePaths">The source files of the difference comparison.</param>
	/// <param name="progress">The callback method to report progress.</param>
	BuildingFacadeSeedRemoval(std::vector<OGRPoint>& seedPoints,
	                          const std::vector<std::string>& sourcePaths,
	                          Operation::ProgressType progress = nullptr,
							  int threshold = 20)
		: DatasetCalculation<DataType>(sourcePaths, nullptr, progress),
		    seedPoints(seedPoints),
			threshold(threshold)
	{
		initialize();
	}

	BuildingFacadeSeedRemoval(const BuildingFacadeSeedRemoval&) = delete;
	BuildingFacadeSeedRemoval& operator=(const BuildingFacadeSeedRemoval&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};

template <typename DataType>
void BuildingFacadeSeedRemoval<DataType>::initialize()
{
	this->computation = [this](int x, int y)
	{
		std::vector<int> idxs;
		int c = 0;
		for (auto& point: seedPoints)
		{
			int counter = 0;
			int ws = 3; // window size = ws*2 + 1
			int px = point.getX();
			int py = point.getY();
			for(int i = px - ws; i <= px + ws; i++){
				for(int j = py - ws; j <= py + ws; j++){
					if(!this->hasSourceData(0,i,j) && this->sourceData(1,i,j) > 10){
						counter++;
					}
				}
			}
			if(counter > threshold){
				idxs.push_back(c);
			}
			c++;
		}
		for (int i = idxs.size() - 1; i >= 0; --i) {
			seedPoints.erase(seedPoints.begin() + idxs[i]);
		}
	};
}
} // CloudTools
