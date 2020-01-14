#include "ContourConvexHullRasterizer.h"

namespace AHN
{
namespace Buildings
{
void ContourConvexHullRasterizer::initialize()
{
	this->nodataValue = 0;

	this->computation = [this](int sizeX, int sizeY)
	{
		cv::Mat contour = cv::Mat::zeros(sizeY, sizeX, CV_8UC3);
		std::vector<std::vector<cv::Point> > chulls(_contours.size());
		for (size_t i = 0; i < _contours.size(); i++)
		{
			convexHull(_contours[i], chulls[i], false);
			// std::uniform_int_distribution
			cv::Scalar color = cv::Scalar((std::rand() + 1) & 255, (std::rand() + 1) & 255, (std::rand() + 1) & 255);
			drawContours(contour, chulls, static_cast<int>(i), color, cv::FILLED, cv::LINE_8);
			//drawContours(contour, contours, static_cast<int>(i), color, 1, cv::LINE_8);
		}

		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				//uchar isEdge = edge.at<uchar>(j, i);
				//this->setTargetData(i, j, static_cast<GByte>(isEdge > 0 ? 255 : this->nodataValue));
				cv::Vec3b rgb = contour.at<cv::Vec3b>(j, i);
				if (rgb == cv::Vec3b::zeros())
					this->setTargetData(i, j, this->nodataValue);
				else
					this->setTargetData(i, j, rgb[0] * 1000000 + rgb[1] * 1000 + rgb[2]);
			}
	};
}

} // Buildings
} // AHN
