#include <cmath>

#include <CloudLib.DEM/Window.h>
#include "NoiseFilter.h"

using namespace CloudLib::DEM;

namespace AHN
{
namespace Buildings
{
NoiseFilter::NoiseFilter(GDALDataset* sourceDataset,
                         const std::string& targetPath,
                         int range,
                         ProgressType progress)
	: SweepLineTransformation<float>({sourceDataset}, targetPath, range, nullptr, progress)
{
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
						noise += std::abs(source.data() - source.data(i, j));
						++counter;
					}

			if (counter == 0) return static_cast<float>(this->nodataValue);
			if (noise / counter > this->threshold) return static_cast<float>(this->nodataValue);
			else return source.data();
		};
	this->nodataValue = 0;
}
} // Buildings
} // AHN
