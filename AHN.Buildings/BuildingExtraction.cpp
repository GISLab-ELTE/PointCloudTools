#include <CloudLib.DEM/Window.h>
#include "BuildingExtraction.h"

using namespace CloudLib::DEM;

namespace AHN
{
namespace Buildings
{
BuildingExtraction::BuildingExtraction(GDALDataset* surfaceDataset, GDALDataset* terrainDataset,
                                       const std::string& targetPath,
                                       ProgressType progress)
	: SweepLineTransformation<GByte, float>(std::vector<GDALDataset*>{surfaceDataset, terrainDataset},
	                                        targetPath, 0, nullptr, progress)
{
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& surface = sources[0];
			const Window<float>& terrain = sources[1];

			if (!terrain.hasData() && surface.hasData())
				return static_cast<GByte>(255);
			return static_cast<GByte>(this->nodataValue);
		};
	this->nodataValue = 0;
}
} // Buildings
} // AHN
