#pragma once

#include <string>
#include <vector>

#include <CloudLib.DEM/SweepLineTransformation.h>
#include <CloudLib.DEM/Window.h>

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a difference comparison for DEM datasets.
/// </summary>
template <typename DataType = float>
class Comparison : public CloudLib::DEM::SweepLineTransformation<DataType>
{
public:	
	double maximumThreshold = 1000;
	double minimumThreshold = 0;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourcePaths">The source files of the comparison.</param>
	/// <param name="targetPath">The target file of the comparison.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Comparison(const std::vector<std::string>& sourcePaths,
	           const std::string& targetPath,
		       CloudLib::DEM::Operation::ProgressType progress = nullptr);
	Comparison(const Comparison&) = delete;
};

template <typename DataType>
Comparison<DataType>::Comparison(const std::vector<std::string>& sourcePaths,
                                 const std::string& targetPath,
	                             CloudLib::DEM::Operation::ProgressType progress)
	: CloudLib::DEM::SweepLineTransformation<DataType>(sourcePaths, targetPath, nullptr, progress)
{
	this->computation = [this](int x, int y, const std::vector<CloudLib::DEM::Window<DataType>>& sources)
	{
		if (!sources[0].hasData() || !sources[1].hasData())
			return static_cast<DataType>(this->nodataValue);

		DataType difference = sources[1].data() - sources[0].data();
		if (std::abs(difference) >= this->maximumThreshold || std::abs(difference) <= this->minimumThreshold)
			difference = static_cast<DataType>(this->nodataValue);
		return difference;
	};
}
} // DEM
} // CloudTools
