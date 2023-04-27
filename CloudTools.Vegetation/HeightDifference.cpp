#include "HeightDifference.h"

namespace CloudTools
{
namespace Vegetation
{
void HeightDifference::calculateDifference()
{
	std::map<std::pair<GUInt32, GUInt32>, double> differences;
	OGRPoint highestA, highestB;
	double diff;
	for (const auto& elem : distance->closest())
	{
		highestA = clusterMapA.highestPoint(elem.first.first);
		highestB = clusterMapB.highestPoint(elem.first.second);

		diff = highestB.getZ() - highestA.getZ();
		differences.insert(std::make_pair(elem.first, diff));
	}
}
} // Vegetation
} // CloudTools