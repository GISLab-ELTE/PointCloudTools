#pragma once

#include <cmath>
#include <string>
#include <algorithm>

#include "../SweepLineTransformation.h"
#include "../Window.h"

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a noise filter for DEM datasets.
/// </summary>
template <typename DataType = float>
class NoiseFilter : public SweepLineTransformation<DataType>
{
public:	
	/// <summary>
	/// The threshold of noise in percentage (between values 0 and 1).
	/// </summary>
	/// <remarks>
	/// Noise is the average percentage of difference compared to the surrounding area.
	/// </remarks>
	double threshold = 0.5;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the filter.</param>
	/// <param name="targetPath">The target file of the filter.</param>
	/// <param name="range">The range of surrounding data to involve.</param>
	/// <param name="progress">The callback method to report progress.</param>
	NoiseFilter(const std::string& sourcePath,
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
	NoiseFilter(GDALDataset* sourceDataset,
		        const std::string& targetPath,
		        int range,
				Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<DataType>({ sourceDataset }, targetPath, range, nullptr, progress)
	{
		initialize();
	}

	NoiseFilter(const NoiseFilter&) = delete;
	NoiseFilter& operator=(const NoiseFilter&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};

template <typename DataType>
void NoiseFilter<DataType>::initialize()
{
	// Noise is the average percentage of difference compared to the surrounding area.
	this->computation = [this](int x, int y, const std::vector<Window<DataType>>& sources)
	{
		const Window<DataType>& source = sources[0];
		if (!source.hasData()) return static_cast<DataType>(this->nodataValue);

		float noise = 0;
		int counter = -1;
		for (int i = -this->range(); i <= this->range(); ++i)
			for (int j = -this->range(); j <= this->range(); ++j)
				if (source.hasData(i, j))
				{
					noise += std::abs(source.data() - source.data(i, j))
						/ std::min(std::abs(source.data()), std::abs(source.data(i, j)));
					++counter;
				}

		if (counter == 0) return static_cast<DataType>(this->nodataValue);
		if (noise / counter > this->threshold) return static_cast<DataType>(this->nodataValue);
		else return source.data();
	};
	this->nodataValue = 0;
}
} // DEM
} // CloudTools
