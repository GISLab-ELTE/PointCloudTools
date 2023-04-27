#include "ContourSimplification.h"

namespace AHN
{
namespace Buildings
{
void ContourSimplification::onPrepare()
{
	_outputContours.reserve(_contours.size());
}

void ContourSimplification::onExecute()
{
	for (auto const &straightSegments : _contours)
	{
		// filtering identical segments
		std::vector<std::vector<cv::Point> > uniqueSegments;
		uniqueSegments.reserve(straightSegments.size());
		for (auto it1 = straightSegments.begin(); it1 != straightSegments.end(); ++it1)
		{
			bool keep = true;
			for (auto it2 = straightSegments.begin(); it2 != it1; ++it2)
			{
				auto const &e11 = *(it1->begin());
				auto const &e12 = *(it1->rbegin());
				auto const &e21 = *(it2->begin());
				auto const &e22 = *(it2->rbegin());
				if ((cv::norm(e11 - e21) < maxSegmentEndpointPixelDifference && cv::norm(e12 - e22) < maxSegmentEndpointPixelDifference ||
					 cv::norm(e11 - e22) < maxSegmentEndpointPixelDifference && cv::norm(e12 - e21) < maxSegmentEndpointPixelDifference) &&
					(abs(cv::norm(e11 - e12) - cv::norm(e21 - e22)) < maxSegmentNormDifference))
				{
					keep = false; break;
				}
				int cnt = std::count_if(it1->begin(), it1->end(),
					[&it2](const cv::Point &x) { return std::find(it2->begin(), it2->end(), x) != it2->end(); });
				if (cnt > it1->size() * minRatio_identicalPoints)
				{
					keep = false; break;
				}
			}
			if (keep) uniqueSegments.push_back(*it1);
		}
		uniqueSegments.shrink_to_fit();

		// merging segments with similar direction
		std::vector<std::vector<cv::Point> > groupedSegments;
		groupedSegments.reserve(uniqueSegments.size());
		for (auto it = uniqueSegments.begin(); it < uniqueSegments.end(); )
		{
			int cnt = 0, sum = it->size();
			bool stop = false;
			auto const &vec1 = *(it->begin()) - *(it->rbegin());
			while (!stop && it + cnt < uniqueSegments.end() - 1)
			{
				++cnt;
				auto const &vec2 = *((it + cnt)->begin()) - *((it + cnt)->rbegin());
				stop = abs(atan2(vec1.y, vec1.x) - atan2(vec2.y, vec2.x)) > similarAngleThreshold;
				if (!stop) sum += (it + cnt)->size();
			}

			if (cnt > 1)
			{
				std::vector<cv::Point> v;
				v.reserve(sum);
				std::for_each(it, it + cnt, [&v](const std::vector<cv::Point> &x)
				{ v.insert(v.end(), x.begin(), x.end()); });
				groupedSegments.push_back(v);
			}
			else
				groupedSegments.push_back(*it);

			it += cnt;
			if (!cnt) break;
		}

		auto longest = std::max_element(groupedSegments.begin(), groupedSegments.end(),
			[](const std::vector<cv::Point> &x, const std::vector<cv::Point> &y) { return x.size() < y.size(); });
		if (longest->size() > 2 * groupedSegments.begin()->size())
		{
			std::vector<std::vector<cv::Point> > tmp;
			std::reverse_copy(groupedSegments.begin(), longest, std::back_inserter(tmp));
			groupedSegments.erase(groupedSegments.begin(), longest);
			groupedSegments.insert(groupedSegments.end(), tmp.begin(), tmp.end());
		}

		groupedSegments.shrink_to_fit();
		_outputContours.push_back(groupedSegments);
	}
}

std::vector<std::vector<std::vector<cv::Point> > >& ContourSimplification::getContours()
{
	return _outputContours;
}
} // Buildings
} // AHN
