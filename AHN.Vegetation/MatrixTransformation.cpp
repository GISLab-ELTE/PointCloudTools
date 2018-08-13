#include <CloudTools.DEM/Window.h>
#include <iostream>

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
	this->middle = 2;
	this->corner = 1;

	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData()) return static_cast<float>(this->nodataValue);

		float value = 0;
		int counter = 0;
		for(int i = -this->range(); i <= this->range(); ++i)
			for(int j = -this->range(); j <= this->range(); ++j)
				if (source.hasData(i, j))
				{
					if (std::abs(i) == 1 && std::abs(j) == 1)
					{
						value += (source.data(i, j) * this->corner);
						counter += this->corner;
					}
					else if (i == 0 && j == 0)
					{
						value += (source.data(i, j) * this->center);
						counter += this->center;
					}
					else
					{
						value += (source.data(i, j) * this->middle);
						counter += this->middle;
					}
				}

		if(value == 0)
			return static_cast<float>(this->nodataValue);

		return value / counter;
	};
	this->nodataValue = 0;
}
}
}