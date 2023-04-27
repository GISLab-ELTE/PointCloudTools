#include "ContourSplitting.h"

namespace AHN
{
namespace Buildings
{
void ContourSplitting::onPrepare()
{
	_segmentedContours.reserve(_contours.size());
}

void ContourSplitting::onExecute()
{
	for (auto const &contour : _contours)
	{
		std::vector<std::vector<cv::Point> > straightSegments;
		std::vector<cv::Point> segment = { contour[0] };
		cv::Point refDir = contour[1] - contour[0];
		bool diverge1 = false, diverge2 = false;
		bool hasXparallel = false, hasYparallel = false;
		int xParallelCnt = 0, yParallelCnt = 0;
		for (auto it = contour.begin() + 2; it != contour.end(); ++it)
		{
			segment.push_back(*(it - 1));
			cv::Point currDir = *it - *(it - 1);

			if (currDir.x == 0) xParallelCnt++;
			else
			{
				hasXparallel = hasXparallel || xParallelCnt > minStraightSegmentLength;
				xParallelCnt = 0;
			}
			if (currDir.y == 0) yParallelCnt++;
			else
			{
				hasYparallel = hasYparallel || yParallelCnt > minStraightSegmentLength;
				yParallelCnt = 0;
			}

			if (refDir.x == 0)
			{
				diverge1 = diverge1 || currDir.x < 0;
				diverge2 = diverge2 || currDir.x > 0;
			}
			if (refDir.y == 0)
			{
				diverge1 = diverge1 || currDir.y < 0;
				diverge2 = diverge2 || currDir.y > 0;
			}

			if (diverge1 && diverge2 || refDir.x * currDir.x < 0 || refDir.y * currDir.y < 0)
			{
				if (hasXparallel && hasYparallel)
				{
					xParallelCnt = 0; yParallelCnt = 0;
					std::vector<cv::Point> inner;

					int parallelCnt = 0;
					for (auto innerIt = segment.begin() + 1; innerIt < segment.end(); ++innerIt)
					{
						inner.push_back(*(innerIt - 1));
						cv::Point innerDir = *innerIt - *(innerIt - 1);

						if (innerDir.x == 0) xParallelCnt++;
						else if (xParallelCnt > minStraightSegmentLength)
						{
							xParallelCnt = 0;
							straightSegments.push_back(inner);
							inner.clear();
							continue;
						}
						else xParallelCnt = 0;

						if (innerDir.y == 0) yParallelCnt++;
						else if (yParallelCnt > minStraightSegmentLength)
						{
							yParallelCnt = 0;
							straightSegments.push_back(inner);
							inner.clear();
							continue;
						}
						else yParallelCnt = 0;
					}
					inner.push_back(*segment.rbegin());
					if (inner.size() > 1) straightSegments.push_back(inner);
					else straightSegments.back().push_back(inner[0]);
				}
				else
				{
					if (segment.size() > 1) straightSegments.push_back(segment);
					else straightSegments.back().push_back(segment[0]);
				}

				hasXparallel = false; hasYparallel = false;
				xParallelCnt = 0; yParallelCnt = 0;
				diverge1 = false; diverge2 = false;
				segment.clear();

				if (it != contour.end() - 1) refDir = *(it + 1) - *it;
			}
		}
		segment.push_back(*contour.rbegin());
		if (segment.size() > 1) straightSegments.push_back(segment);
		else straightSegments.back().push_back(segment[0]);

		_segmentedContours.push_back(straightSegments);
	}
}

std::vector<std::vector<std::vector<cv::Point> > >& ContourSplitting::getContours()
{
	return _segmentedContours;
}
} // Buildings
} // AHN
