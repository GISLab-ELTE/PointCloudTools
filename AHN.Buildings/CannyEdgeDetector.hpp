#include <cmath>
#include <vector>
#include <map>
#include <unordered_set>
#include <algorithm>

#include <opencv2/imgproc.hpp>
#include <gdal_priv.h>

#include <CloudTools.DEM/DatasetTransformation.hpp>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents an edge detection operator using the Canny edge detector
/// https://en.wikipedia.org/wiki/Canny_edge_detector
/// </summary>
template <typename DataType = float>
class CannyEdgeDetector : public CloudTools::DEM::DatasetTransformation<uint32_t, DataType>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source path of the algorithm.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="progress">The callback method to report progress.</param>
	CannyEdgeDetector(GDALDataset* sourceDataset,
					  const std::string& targetPath,
                      CloudTools::Operation::ProgressType progress = nullptr) // TODO: try to use progress properly
		: CloudTools::DEM::DatasetTransformation<uint32_t, DataType>({ sourceDataset }, targetPath, nullptr, progress)
	{
		initialize();
	}

	CannyEdgeDetector(const CannyEdgeDetector&) = delete;
	CannyEdgeDetector& operator=(const CannyEdgeDetector&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();

	std::vector<std::vector<cv::Point> > mergeContours(const std::vector<std::vector<cv::Point> >& contours);
	bool shouldMerge(const std::vector<cv::Point>& c1, const std::vector<cv::Point>& c2);
};

template <typename DataType>
void CannyEdgeDetector<DataType>::initialize()
{
	this->nodataValue = 0;

	this->computation = [this](int sizeX, int sizeY)
	{
		cv::Mat edge;
		edge.create(sizeY, sizeX, CV_8UC1); // required format for the Canny function

		// auto m1 = _sourceDatasets[0]->GetRasterBand(1)->GetMinimum();
		// auto m2 = _sourceDatasets[0]->GetRasterBand(1)->GetMaximum();
		float min = 100000, max = 0;
		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				if (this->hasSourceData(i, j))
				{
					float d = this->sourceData(i, j);
					if (d < min) min = d;
					if (d > max) max = d;
				}
			}

		// transforming values into 0-255 range
		float interval = max - min;
		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				if (this->hasSourceData(i, j))
				{
					float value = this->sourceData(i, j);
					uchar rounded = std::round((value - min) / interval * 255);
					edge.at<uchar>(j, i) = rounded;
				}
				else
					edge.at<uchar>(j, i) = 0;
			}

		// smooth elevation image for better edge results
		// TODO: Gaussian vs box filter
		cv::blur(edge, edge, cv::Size(3,3));
		// TODO: configuring parameter values (fields or command line arguments)
		int upperThreshold = (255 / (max - min)) * 2 * 2;
		cv::Canny(edge, edge,
				  upperThreshold / 3,
				  upperThreshold, // recommended ratio is 1:3 (or 1:2)
				  3, // equal kernel size with smoothing
				  false); // more accurate computation

		std::vector<std::vector<cv::Point> > contours;
		// std::vector<cv::Vec4i> hierarchy;
		//cv::Mat contour = cv::Mat::zeros(edge.rows, edge.cols, CV_8UC1);
		cv::Mat contour = cv::Mat::zeros(edge.rows, edge.cols, CV_8UC3);
		findContours(edge, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
		contours = mergeContours(contours);

		//drawContours(contour, contours, -1, cv::Scalar(255), 1, cv::LINE_8);
		//drawContours(contour, contours, -1, cv::Scalar(255), cv::FILLED, cv::LINE_8);

		std::vector<std::vector<cv::Point> > chulls(contours.size());
		for (size_t i = 0; i < contours.size(); i++)
		{
			convexHull(contours[i], chulls[i], false);
			// std::uniform_int_distribution
			cv::Scalar color = cv::Scalar(std::rand()&255, std::rand()&255, std::rand()&255);
			drawContours(contour, chulls, static_cast<int>(i), color, cv::FILLED, cv::LINE_8);
			//drawContours(contour, contours, static_cast<int>(i), color, 1, cv::LINE_8);
		}

		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				//uchar isEdge = edge.at<uchar>(j, i);
				//this->setTargetData(i, j, static_cast<GByte>(isEdge > 0 ? 255 : this->nodataValue));
				cv::Vec3b rgb = contour.at<cv::Vec3b>(j, i);
				this->setTargetData(i, j, rgb[0]*1000000 + rgb[1]*1000 + rgb[2]);
			}

		//for (auto const &contour : contours)
		//{
		//	uint32_t rnd = std::rand() % UINT32_MAX;
		//	for (auto const &point : contour)
		//		this->setTargetData(point.x, point.y, rnd);
		//}
	};
}

template <typename DataType>
std::vector<std::vector<cv::Point> > CannyEdgeDetector<DataType>::mergeContours(const std::vector<std::vector<cv::Point> >& contours)
{
	std::map<uint, uint> segmentInContour;
	std::map<uint, std::vector<uint> > contourSegments;
	std::map<uint, std::vector<cv::Point> > contourPoints;
	std::vector<std::vector<cv::Point> > filteredContours;
	filteredContours.reserve(contours.size());

	// TODO: storing contours being filtered out too?
	std::copy_if(contours.begin(), contours.end(), std::back_inserter(filteredContours),
		[](const std::vector<cv::Point>& c) {
			// possible connection between redundancy and geometry
			//auto hash = [](const cv::Point& p) { return (size_t)(p.x * 100 + p.y); };
			//auto eq = [](const cv::Point& p1, const cv::Point& p2) { return p1 == p2; };
			//std::unordered_set<cv::Point, decltype(hash), decltype(eq)> set(c.begin(), c.end(), 11, hash, eq);
			//bool redundancy = set.size() * 1.6 < c.size() || set.size() * 1.1 > c.size();

			int minStraightLength = 20;
			auto pred = [&minStraightLength, it = c.begin()](const cv::Point& p) mutable {
				auto interval = (it++) + minStraightLength;
				auto bp = p, ep = *interval;
				return all_of(it, interval, [bp, ep](const cv::Point& p) {
					return p.x - std::min(bp.x, ep.x) * p.x - std::max(bp.x, ep.x) <= 0 &&
						   p.y - std::min(bp.y, ep.y) * p.y - std::max(bp.y, ep.y) <= 0;
				});
			};
			bool monoton = c.size() >= minStraightLength &&
				std::count_if(c.begin(), c.end() - minStraightLength, pred) * 1.2 > c.size();
			minStraightLength = 100;
			bool hasStraight = c.size() >= minStraightLength &&
				std::any_of(c.begin(), c.end() - minStraightLength, pred);

			return monoton || hasStraight;
		});
	filteredContours.shrink_to_fit();

	std::vector<std::vector<cv::Point> > splittedContours;
	splittedContours.reserve(filteredContours.size());
	for (auto const &contour : filteredContours)
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
				hasXparallel = hasXparallel || xParallelCnt > 5;
				xParallelCnt = 0;
			}
			if (currDir.y == 0) yParallelCnt++;
			else
			{
				hasYparallel = hasYparallel || yParallelCnt > 5;
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
						else if (xParallelCnt > 5)
						{
							xParallelCnt = 0;
							straightSegments.push_back(inner);
							inner.clear();
							continue;
						}
						else xParallelCnt = 0;

						if (innerDir.y == 0) yParallelCnt++;
						else if (yParallelCnt > 5)
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

				if (it != contour.end()-1) refDir = *(it + 1) - *it;
			}
		}
		segment.push_back(*contour.rbegin());
		if (segment.size() > 1) straightSegments.push_back(segment);
		else straightSegments.back().push_back(segment[0]);

		//std::copy(straightSegments.begin(), straightSegments.end(), std::back_inserter(splittedContours));
		//continue;

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
				if ((cv::norm(e11 - e21) < 7 && cv::norm(e12 - e22) < 7 || cv::norm(e11 - e22) < 7 && cv::norm(e12 - e21) < 7) &&
					(abs(cv::norm(e11 - e12) - cv::norm(e21 - e22)) < 10))
				{
					keep = false; break;
				}
				int cnt = std::count_if(it1->begin(), it1->end(),
					[&it2](const cv::Point &x) { return std::find(it2->begin(), it2->end(), x) != it2->end(); });
				if (cnt > it1->size() / 2)
				{
					keep = false; break;
				}
			}
			if (keep) uniqueSegments.push_back(*it1);
		}

		//std::copy(uniqueSegments.begin(), uniqueSegments.end(), std::back_inserter(splittedContours));
		//continue;

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
				stop = abs(atan2(vec1.y, vec1.x) - atan2(vec2.y, vec2.x)) > 3.14 / 16;
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

		//std::copy(groupedSegments.begin(), groupedSegments.end(), std::back_inserter(splittedContours));
		//continue;

		auto longest = std::max_element(groupedSegments.begin(), groupedSegments.end(),
			[](const std::vector<cv::Point> &x, const std::vector<cv::Point> &y) { return x.size() < y.size(); });
		if (longest->size() > 2 * groupedSegments.begin()->size())
		{
			std::vector<std::vector<cv::Point> > tmp;
			std::reverse_copy(groupedSegments.begin(), longest, std::back_inserter(tmp));
			groupedSegments.erase(groupedSegments.begin(), longest);
			groupedSegments.insert(groupedSegments.end(), tmp.begin(), tmp.end());
		}

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
			if (cv::norm(ce1 - ce2) < 5 && it > groupedSegments.begin() + 1) break;

			sumCnt += it->size();

			auto const &e1 = *(it->begin());
			auto const &e2 = *(it->rbegin());
			float currAngle = atan((e1 - e2).y / (float)(e1 - e2).x);
			bool reverse = abs(refAngle - currAngle) < 3.14 / 18;
			float old = refAngle;
			refAngle = currAngle;
			//++it;

			if (cv::norm(e1 - ce1) < 15 && (!reverse || extended != 1))
			{
				ce1 = e2;
				extended = 1;
				std::copy(it->begin(), it->end(), std::back_inserter(part1));
				partCnt += it->size();
				++it;
			}
			else if (cv::norm(e1 - ce2) < 15 && (!reverse || extended != 2))
			{
				ce2 = e2;
				extended = 2;
				std::copy(it->begin(), it->end(), std::back_inserter(part2));
				partCnt += it->size();
				++it;
			}
			else if (cv::norm(e2 - ce1) < 15 && (!reverse || extended != 1))
			{
				ce1 = e1;
				extended = 1;
				std::copy(it->begin(), it->end(), std::back_inserter(part1));
				partCnt += it->size();
				++it;
			}
			else if (cv::norm(e2 - ce2) < 15 && (!reverse || extended != 2))
			{
				ce2 = e1;
				extended = 2;
				std::copy(it->begin(), it->end(), std::back_inserter(part2));
				partCnt += it->size();
				++it;
			}
			else
			{
				//it = groupedSegments.erase(it);
				++it;
				refAngle = old;
			}
		}

		//bool simple = groupedSegments.size() < 10;
		//bool hasLongSegment = std::find_if(groupedSegments.begin(), groupedSegments.end(),
		//	[](const std::vector<cv::Point>& s) { return s.size() > 25; }) != groupedSegments.end();
		bool size = sumCnt > 80; // at least 40 meters periphery
		if (size)
		{
			// vegetation filter
			std::vector<cv::Point> finalContour;

			const int partitionNr = 8;
			bool flags[partitionNr] = { false };
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
						index = (angle + CV_PI / 2) * partitionNr / CV_PI;
					}
					flags[index] = true;
				}
				if (abs(diff.x) + abs(diff.y) < segment.size()) complexSegmentCnt++;
				if (segment.size() > 15) longSegmentCnt++;
			}

			int cond1 = std::count(flags, flags + partitionNr, true) >= partitionNr * 0.75;
			int cond2 = complexSegmentCnt > groupedSegments.size() * 0.25;
			int cond3 = longSegmentCnt / (float)groupedSegments.size() < 0.25;
			int cond4 = sumCnt / groupedSegments.size() < 15;

			if (cond1 + cond2 + cond3 + cond4 < 2)
			{
				finalContour.reserve(part1.size() + part2.size());
				std::reverse_copy(part1.begin(), part1.end(), std::back_inserter(finalContour));
				std::copy(part2.begin(), part2.end(), std::back_inserter(finalContour));

				if (partCnt * 1.33 > sumCnt && cv::norm(finalContour.front() - finalContour.back()) < 20)
				{
					// successful segment chaining
					splittedContours.push_back(finalContour);
				}
				else
				{
					finalContour.clear();
					std::for_each(groupedSegments.begin(), groupedSegments.end(),
						[&finalContour](const std::vector<cv::Point> &x)
							{ finalContour.insert(finalContour.end(), x.begin(), x.end()); });
					splittedContours.push_back(finalContour);
				}
			}
		}
	}
	splittedContours.shrink_to_fit();

	return splittedContours;

	///*
	//for (uint i = 0; i < filteredContours.size(); ++i)
	//{
	//	segmentInContour[i] = i;
	//	contourSegments[i] = { i };
	//	contourPoints[i] = contours[i]; // TODO: move?
	//}

	//bool modified = true;
	//while (modified)
	//{
	//	modified = false;
	//	std::vector<uint> mergedSegments;
	//	for (auto contour1 : contourPoints) // iter
	//	{
	//		auto c1 = contour1.second;
	//		bool tmp = c1[0].x > 2090 && c1[0].x < 2175 && c1[0].y > 2590 && c1[0].y < 2665;
	//		/*for (auto contour2 : contourPoints)
	//		{
	//			if (contour1.first != contour2.first && shouldMerge(contour1.second, contour2.second))
	//			{
	//				mergedSegments.push_back(contour2.first);
	//				contour1.second.insert(contour1.second.end(), contour2.second.begin(), contour2.second.end()); // += gap
	//				contourSegments[contour1.first].insert(contourSegments[contour1.first].end(),
	//					contourSegments[contour2.first].begin(), contourSegments[contour2.first].end());
	//				for (auto id : contourSegments[contour2.first])
	//					segmentInContour[id] = contour1.first;
	//			}
	//		}*/
	//	}
	//	if (mergedSegments.size() > 0)
	//	{
	//		modified = true;
	//		for (auto id : mergedSegments)
	//		{
	//			contourSegments.erase(id);
	//			contourPoints.erase(id);
	//		}
	//	}
	//}
	//return contours;
	//*/
}
template<typename DataType>
bool CannyEdgeDetector<DataType>::shouldMerge(const std::vector<cv::Point>& c1, const std::vector<cv::Point>& c2)
{
	cv::Point vec1 = c1[0] - c1[c1.size() - 1];
	cv::Point vec2 = c2[0] - c2[c2.size() - 1];
	/*
	double l1 = cv::arcLength(c1, false);
	double l2 = cv::arcLength(c2, false);
	double a1 = cv::contourArea(c1);
	double a2 = cv::contourArea(c2);
	bool simple1 = cv::norm(vec1) / c1.size() > 0.7;
	bool simple2 = cv::norm(vec2) / c2.size() > 0.7;
	*/
	bool length = c1.size() > 5 && c2.size() > 5;
	bool diff = cv::norm(vec1) > 5 && cv::norm(vec2) > 5;
	bool direction = abs(abs(atan2(vec1.y, vec1.x)) - abs(atan2(vec2.y, vec2.x))) < 3.14 / 8;
	bool nearby = cv::norm(c1[0] - c2[c2.size() - 1]) < 10 || cv::norm(c2[0] - c1[c1.size() - 1]) < 10 ||
		cv::norm(c1[0] - c2[0]) < 10 || cv::norm(c1[c1.size() - 1] - c2[c2.size() - 1]) < 10;

	return length && diff && direction && nearby;
}

} // Buildings
} // AHN
