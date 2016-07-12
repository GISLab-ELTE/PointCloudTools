#include <cmath>

#include <CloudLib.DEM/Window.h>
#include "MorphologyFilter.h"

using namespace CloudLib::DEM;

namespace AHN
{
namespace Buildings
{
MorphologyFilter::MorphologyFilter(GDALDataset* sourceDataset,
                                   const std::string& targetPath,
                                   Method method,
                                   ProgressType progress)
	: SweepLineTransformation<float>({sourceDataset}, targetPath, 1, nullptr, progress),
	  method(method)
{
	// https://en.wikipedia.org/wiki/Mathematical_morphology
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];

			float sum = 0;
			int counter = 0;
			for (int i = -1; i <= 1; ++i)
				for (int j = -1; j <= 1; ++j)
					if (source.hasData(i, j))
					{
						sum += source.data(i, j);
						++counter;
					}

			if (this->method == Method::Dilation && !source.hasData() && counter > 0)
				return sum / counter;
			if (this->method == Method::Erosion && source.hasData() && counter == 1)
				return static_cast<float>(this->nodataValue);
			return source.hasData() ? source.data() : static_cast<float>(this->nodataValue);
		};
	this->nodataValue = 0;
}
} // Buildings
} // AHN
