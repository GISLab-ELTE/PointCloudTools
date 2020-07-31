#include <cmath>
#include <algorithm>

#include <CloudTools.DEM/Window.hpp>

#include "NoiseFilter.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
NoiseFilter::NoiseFilter(GDALDataset* sourceDataset,
                         const std::string& targetPath,
                         int range,
                         ProgressType progress)
	: SweepLineTransformation<float>({sourceDataset}, targetPath, range, nullptr, progress)
{
	// Noise is the average percentage of difference compared to the surrounding area.
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData()) return static_cast<float>(this->nodataValue);

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

		if (counter == 0)
			return source.data();
		if (noise / counter >= this->threshold)
			return source.data();
		else
			return static_cast<float>(this->nodataValue);
	};
	this->nodataValue = 0;
}
} // Vegetation
} // AHN
