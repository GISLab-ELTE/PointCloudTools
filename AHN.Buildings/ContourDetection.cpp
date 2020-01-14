#include "ContourDetection.h"

namespace AHN
{
namespace Buildings
{
void ContourDetection::initialize()
{
	this->computation = [this](int sizeX, int sizeY)
	{
		cv::Mat edge;
		edge.create(sizeY, sizeX, CV_8UC1); // required format for the Canny function

		float min = FLT_MAX, max = 0;
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
		cv::blur(edge, edge, cv::Size(smoothingFilterKernelSize,smoothingFilterKernelSize));
		cannyUpperThreshold = (255 / (max - min)) * 2 * 2;
		cv::Canny(edge, edge,
				  cannyUpperThreshold / cannyThresholdRatio,
				  cannyUpperThreshold, // recommended ratio is 1:3 (or 1:2)
				  smoothingFilterKernelSize, // equal kernel size with smoothing
				  false); // more accurate computation

		findContours(edge, this->_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
	};
}

std::vector<std::vector<cv::Point> >& ContourDetection::getContours()
{
	return _contours;
}
} // Buildings
} // AHN
