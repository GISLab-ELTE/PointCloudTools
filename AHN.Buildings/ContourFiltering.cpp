#include "ContourFiltering.h"

namespace AHN
{
namespace Buildings
{
void ContourFiltering::onPrepare()
{
	_filteredContours.reserve(_contours.size());
}

void ContourFiltering::onExecute()
{
	std::copy_if(_contours.begin(), _contours.end(), std::back_inserter(_filteredContours),
		[this](const std::vector<cv::Point>& c) {
		int minStraightLength = this->subsequenceLength_monotonic;
		auto pred = [&minStraightLength, it = c.begin()](const cv::Point& p) mutable {
			auto interval = it + minStraightLength;
			auto bp = p, ep = *interval;
			return std::all_of(it++, interval, [bp, ep](const cv::Point& p) {
				// axis-aligned bounding box
				return (p.x - std::min(bp.x, ep.x)) * (p.x - std::max(bp.x, ep.x)) <= 0 &&
					(p.y - std::min(bp.y, ep.y)) * (p.y - std::max(bp.y, ep.y)) <= 0;
			});
		};
		bool monoton = c.size() >= minStraightLength &&
			std::count_if(c.begin(), c.end() - minStraightLength, pred) > c.size() * this->minRatio_monotonic;
		minStraightLength = this->subsequenceLength_straight;
		bool hasStraight = c.size() >= minStraightLength &&
			std::any_of(c.begin(), c.end() - minStraightLength, pred);

		return monoton || hasStraight;
	});
	_filteredContours.shrink_to_fit();
}

std::vector<std::vector<cv::Point> >& ContourFiltering::getContours()
{
	return _filteredContours;
}
} // Buildings
} // AHN
