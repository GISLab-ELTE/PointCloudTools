#include <CloudTools.DEM/Window.h>

#include "MatrixTransformation.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Vegetation
{
MatrixTransformation::MatrixTransformation(GDALDataset *sourceDataset,
										   const std::string& targetPath,
									 	   int range,
										   ProgressType progress)
	: SweepLineTransformation<float>({ sourceDataset }, targetPath, range, nullptr, progress)
{
	this->center = 4;
	this->middle = 1;
	this->corner = -1;

	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData()) return static_cast<float>(this->nodataValue);

		float value = 0;
		int counter = -1;
		for(int i = -this->range(); i <= this->range(); ++i)
			for(int j = -this->range(); j <= this->range(); ++j)
				if (source.hasData(i, j))
				{
					if (i != 0 && j != 0)
					{
						value = source.data(i, j) * this->corner;
					}
					else if (i == 0 && j == 0)
					{
						value = source.data(i, j) * this->center;
					}
					else
					{
						value = source.data(i, j) * this->middle;
					}
				}

		return value;
	};
	this->nodataValue = 0;
}
}
}