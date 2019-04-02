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
		// possible connection between redundancy and geometry
		//auto hash = [](const cv::Point& p) { return (size_t)(p.x * 100 + p.y); };
		//auto eq = [](const cv::Point& p1, const cv::Point& p2) { return p1 == p2; };
		//std::unordered_set<cv::Point, decltype(hash), decltype(eq)> set(c.begin(), c.end(), 11, hash, eq);
		//bool redundancy = set.size() * 1.6 < c.size() || set.size() * 1.1 > c.size();

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
