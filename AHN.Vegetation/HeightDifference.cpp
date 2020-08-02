#include "HeightDifference.h"

namespace AHN
{
namespace Vegetation
{
void HeightDifference::calculateDifference()
{
	std::map<std::pair<GUInt32, GUInt32>, double> differences;
	OGRPoint ahn2Highest, ahn3Highest;
	double diff;
	for (const auto& elem : distance->closest())
	{
		ahn2Highest = ahn2.highestPoint(elem.first.first);
		ahn3Highest = ahn3.highestPoint(elem.first.second);

		diff = ahn3Highest.getZ() - ahn2Highest.getZ();
		differences.insert(std::make_pair(elem.first, diff));
	}
}
} // Vegetation
} // AHN