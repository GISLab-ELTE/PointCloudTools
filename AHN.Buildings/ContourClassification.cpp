#include "ContourClassification.h"

namespace AHN
{
namespace Buildings
{
void ContourClassification::onPrepare()
{
	_buildingContours.reserve(_contours.size());
}

void ContourClassification::onExecute()
{
	for (auto const &groupedSegments : _contours)
	{
		auto ce1 = *(groupedSegments.begin()->begin());
		auto ce2 = *(groupedSegments.begin()->rbegin());
		float refAngle = atan((ce1 - ce2).y / (float)(ce1 - ce2).x);
		int extended = 0;
		std::vector<cv::Point> part1;
		std::vector<cv::Point> part2 = *(groupedSegments.begin());
		int partCnt = groupedSegments.begin()->size();
		int sumCnt = groupedSegments.begin()->size();

		for (auto it = groupedSegments.begin() + 1; it < groupedSegments.end(); )
		{
			if (cv::norm(ce1 - ce2) < closurePixelDifference && it > groupedSegments.begin() + 1) break;

			sumCnt += it->size();

			auto const &e1 = *(it->begin());
			auto const &e2 = *(it->rbegin());
			float currAngle = atan((e1 - e2).y / (float)(e1 - e2).x);
			bool reverse = abs(refAngle - currAngle) < similarAngleThreshold;
			float old = refAngle;
			refAngle = currAngle;

			if (cv::norm(e1 - ce1) < chainingPixelDifference && (!reverse || extended != 1))
			{
				ce1 = e2;
				extended = 1;
				std::copy(it->begin(), it->end(), std::back_inserter(part1));
				partCnt += it->size();
				++it;
			}
			else if (cv::norm(e1 - ce2) < chainingPixelDifference && (!reverse || extended != 2))
			{
				ce2 = e2;
				extended = 2;
				std::copy(it->begin(), it->end(), std::back_inserter(part2));
				partCnt += it->size();
				++it;
			}
			else if (cv::norm(e2 - ce1) < chainingPixelDifference && (!reverse || extended != 1))
			{
				ce1 = e1;
				extended = 1;
				std::copy(it->begin(), it->end(), std::back_inserter(part1));
				partCnt += it->size();
				++it;
			}
			else if (cv::norm(e2 - ce2) < chainingPixelDifference && (!reverse || extended != 2))
			{
				ce2 = e1;
				extended = 2;
				std::copy(it->begin(), it->end(), std::back_inserter(part2));
				partCnt += it->size();
				++it;
			}
			else
			{
				++it;
				refAngle = old;
			}
		}

		bool size = sumCnt > minBuildingPixelPerimeter;
		if (size)
		{
			// vegetation filter
			std::vector<cv::Point> finalContour;

			std::vector<bool> flags(contourPartitionNumber, false);
			int complexSegmentCnt = 0;
			int longSegmentCnt = 0;

			for (auto const &segment : groupedSegments)
			{
				cv::Point diff = segment.front() - segment.back();
				if (segment.size() > 2)
				{
					int index = 0;
					if (diff.x > 0)
					{
						float angle = atan(diff.y / (float)diff.x);
						index = (angle + CV_PI / 2) * contourPartitionNumber / CV_PI;
					}
					flags[index] = true;
				}
				if (abs(diff.x) + abs(diff.y) < segment.size()) complexSegmentCnt++;
				if (segment.size() > longSegmentPixelThreshold) longSegmentCnt++;
			}

			int cond1 = std::count(flags.begin(), flags.end(), true) >= contourPartitionNumber * condition1Ratio;
			int cond2 = complexSegmentCnt > groupedSegments.size() * condition2Ratio;
			int cond3 = longSegmentCnt < groupedSegments.size() * condition3Ratio;
			int cond4 = sumCnt / groupedSegments.size() < avgSegmentPixels;

			if (cond1 + cond2 + cond3 + cond4 < 2)
			{
				finalContour.reserve(part1.size() + part2.size());
				std::reverse_copy(part1.begin(), part1.end(), std::back_inserter(finalContour));
				std::copy(part2.begin(), part2.end(), std::back_inserter(finalContour));

				if (partCnt > sumCnt * minChainingRatio && cv::norm(finalContour.front() - finalContour.back()) < maxChainingPixelError)
				{
					// successful segment chaining
					_buildingContours.push_back(finalContour);
				}
				else
				{
					finalContour.clear();
					std::for_each(groupedSegments.begin(), groupedSegments.end(),
						[&finalContour](const std::vector<cv::Point> &x)
					{ finalContour.insert(finalContour.end(), x.begin(), x.end()); });
					_buildingContours.push_back(finalContour);
				}
			}
		}
	}
	_buildingContours.shrink_to_fit();
}

std::vector<std::vector<cv::Point> >& ContourClassification::getContours()
{
	return _buildingContours;
}
} // Buildings
} // AHN
