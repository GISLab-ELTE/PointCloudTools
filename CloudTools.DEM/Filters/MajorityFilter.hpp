#pragma once

#include <cmath>
#include <string>

#include "../Window.h"
#include "../SweepLineTransformation.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a majority filter for DEM datasets.
/// </summary>
template <typename DataType = float>
class MajorityFilter : public SweepLineTransformation<DataType>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="range">The range of surrounding data to involve.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MajorityFilter(const std::string& sourcePath,
	               const std::string& targetPath,
	               int range,
				   Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<DataType>({ sourcePath }, targetPath, range, nullptr, progress)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="range">The range of surrounding data to involve.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MajorityFilter(GDALDataset* sourceDataset,
		           const std::string& targetPath,
		           int range,
				   Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<DataType>({ sourceDataset }, targetPath, range, nullptr, progress)
	{
		initialize();
	}

	MajorityFilter(const MajorityFilter&) = delete;
	MajorityFilter& operator=(const MajorityFilter&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};

template <typename DataType>
void MajorityFilter<DataType>::initialize()
{
	this->computation = [this](int x, int y, const std::vector<Window<DataType>>& sources)
	{
		const Window<DataType>& source = sources[0];

		float sum = 0;
		int counter = 0;
		for (int i = -this->range(); i <= this->range(); ++i)
			for (int j = -this->range(); j <= this->range(); ++j)
				if (source.hasData(i, j))
				{
					sum += source.data(i, j);
					++counter;
				}

		if (counter < (std::pow(this->range() * 2 + 1, 2) / 2)) return static_cast<DataType>(this->nodataValue);
		else return source.hasData() ? source.data() : sum / counter;
	};
	this->nodataValue = 0;
}
} // DEM
} // CloudTools
