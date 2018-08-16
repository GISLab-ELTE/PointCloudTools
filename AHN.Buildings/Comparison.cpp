#include <cmath>

#include <CloudTools.DEM/Window.hpp>
#include "Comparison.h"

using namespace CloudTools::DEM;

namespace AHN
{
namespace Buildings
{
Comparison::Comparison(GDALDataset* ahn2Dataset, GDALDataset* ahn3Dataset,
                       const std::string& targetPath,
                       ProgressType progress)
	: SweepLineTransformation<float>(std::vector<GDALDataset*>{ahn2Dataset, ahn3Dataset}, targetPath, 0, nullptr, progress)
{
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& ahn2 = sources[0];
			const Window<float>& ahn3 = sources[1];

			if (!ahn2.hasData() || !ahn3.hasData())
				return static_cast<float>(this->nodataValue);

			float difference = ahn3.data() - ahn2.data();
			if (std::abs(difference) >= this->maximumThreshold || std::abs(difference) <= this->minimumThreshold)
				difference = static_cast<float>(this->nodataValue);
			return difference;
		};
	this->nodataValue = 0;
}

Comparison::Comparison(GDALDataset* ahn2Dataset, GDALDataset* ahn3Dataset,
                       GDALDataset* ahn2Filter, GDALDataset* ahn3Filter,
                       const std::string& targetPath,
                       ProgressType progress)
	: SweepLineTransformation<float>(std::vector<GDALDataset*>{ahn2Dataset, ahn3Dataset, ahn2Filter, ahn3Filter},
	                                 targetPath, 0, nullptr, progress)
{
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
		{
			const Window<float>& ahn2Data = sources[0];
			const Window<float>& ahn3Data = sources[1];
			const Window<float>& ahn2Filter = sources[2];
			const Window<float>& ahn3Filter = sources[3];

			/*
			 * Since AHN-3 is incomplete, side tiles are partial, 
			 * resulting in false positive detection of mass building demolition
			 * when relying only on the filter laysers.
			 * TODO: this removes demolitions over water (e.g. TU Delft Faculty of Architecture building.)
			 */
			if (!ahn2Filter.hasData() && !ahn3Filter.hasData() ||
				!ahn3Data.hasData())
				return static_cast<float>(this->nodataValue);

			float difference = 0.f;
			if (ahn2Data.hasData() && ahn3Data.hasData())
				difference = ahn3Data.data() - ahn2Data.data();
			else if (ahn2Data.hasData())
				difference = -ahn2Data.data();
			else if (ahn3Data.hasData())
				difference = ahn3Data.data();

			if (std::abs(difference) >= this->maximumThreshold || std::abs(difference) <= this->minimumThreshold)
				difference = static_cast<float>(this->nodataValue);
			return difference;
		};
	this->nodataValue = 0;
}
} // Buildings
} // AHN
