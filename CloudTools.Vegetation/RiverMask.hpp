#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <iostream>

#include <CloudTools.DEM/SweepLineTransformation.hpp>

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a DTM in which rivers are not nodata points, but instead have a constant value of -0.4
/// </summary>
template <typename DataType = float>
class RiverMask : public SweepLineTransformation<DataType>
{
public:
	RiverMask(const std::vector<std::string>& sourcePaths,
	          const std::string& targetPath,
	          Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<DataType>(sourcePaths, targetPath, nullptr, progress)
	{
		initialize();
	}

	RiverMask(const RiverMask&) = delete;
	RiverMask& operator=(const RiverMask&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
        /// <summary>
	/// Extremal low value for rivers.
	/// </summary>
	const int riverHeight = -1000;
};

template <typename DataType>
void RiverMask<DataType>::initialize()
{
	/// returns the DTM value, if both the DTM and the DSM value is given;
	/// returns -0.4 if only the DTM value or neither the DTM and the DSM value is given;
	/// return the DSM value, if only the DSM value is given.
	this->computation = [this](int x, int y, const std::vector<Window<DataType>>& sources)
	{
		if (!sources[0].hasData() && sources[1].hasData())
			return static_cast<DataType>(this->nodataValue);
		if (!sources[1].hasData())
			return static_cast<DataType>(riverHeight);
		if (sources[0].hasData())
			return static_cast<DataType>(sources[0].data());
	};
}
} // DEM
} // CloudTools
