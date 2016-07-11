#include <cmath>

#include <CloudLib.DEM/Window.h>
#include "MajorityFilter.h"

using namespace CloudLib::DEM;

namespace AHN
{
namespace Buildings
{
MajorityFilter::MajorityFilter(GDALDataset* sourceDataset,
                               const std::string& targetPath,
                               int range,
                               ProgressType progress)
	: SweepLineTransformation<float>({sourceDataset}, targetPath, range, nullptr, progress)
{
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];

			float sum = 0;
			int counter = 0;
			for (int i = -this->range(); i <= this->range(); ++i)
				for (int j = -this->range(); j <= this->range(); ++j)
					if (source.hasData(i, j))
					{
						sum += source.data(i, j);
						++counter;
					}
			
			if (counter < (std::pow(this->range() * 2 + 1, 2) / 2)) return static_cast<float>(this->nodataValue);
			else return source.hasData() ? source.data() : sum / counter;
		};
	this->nodataValue = 0;
}
} // Buildings
} // AHN
