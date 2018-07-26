#pragma once

#include <string>
#include <vector>
#include <cmath>

#include "../SweepLineTransformation.h"
#include "../Window.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a difference comparison for DEM datasets.
/// </summary>
template <typename DataType = float>
class Difference : public SweepLineTransformation<DataType>
{
public:	
	double maximumThreshold = 1000;
	double minimumThreshold = 0;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourcePaths">The source files of the difference comparison.</param>
	/// <param name="targetPath">The target file of the difference comparison.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Difference(const std::vector<std::string>& sourcePaths,
	           const std::string& targetPath,
		       Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<DataType>(sourcePaths, targetPath, nullptr, progress)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourceDatasets">The source datasets of the difference comparison.</param>
	/// <param name="targetPath">The target file of the difference comparison.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Difference(const std::vector<GDALDataset*>& sourceDatasets,
		       const std::string& targetPath,
		       Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<DataType>(sourceDatasets, targetPath, nullptr, progress)
	{
		initialize();
	}

	Difference(const Difference&) = delete;
	Difference& operator=(const Difference&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};

template <typename DataType>
void Difference<DataType>::initialize()
{
	this->computation = [this](int x, int y, const std::vector<Window<DataType>>& sources)
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
