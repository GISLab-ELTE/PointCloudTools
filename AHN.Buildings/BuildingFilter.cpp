#include <CloudTools.DEM/Window.h>
#include "BuildingFilter.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Buildings
{
BuildingFilter::BuildingFilter(GDALDataset* sourceDataset,
                               const std::string& targetPath,
                               ProgressType progress)
	: SweepLineTransformation<GByte, float>({sourceDataset}, targetPath, 0, nullptr, progress)
{
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& source = sources[0];
			return static_cast<GByte>(source.hasData() ? 255 : this->nodataValue);
		};
	this->nodataValue = 0;
}
} // Buildings
} // AHN
