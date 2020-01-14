#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

#include <opencv2/imgproc.hpp>

#include <CloudTools.Common/Operation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Removing redundant informations from the represenation of the contours.
/// </summary>
class ContourSimplification: public CloudTools::Operation
{
public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="contours">The input contours.</param>
	ContourSimplification(const std::vector<std::vector<std::vector<cv::Point> > >& contours) : _contours(contours)
	{ }

	/// <summary>
	/// Retrieves the output contours of the operation.
	/// </summary>
	/// <returns>The initial contours without redundancy.</returns>
	std::vector<std::vector<std::vector<cv::Point> > >& getContours();

	int maxSegmentEndpointPixelDifference = 7;
	int maxSegmentNormDifference = 10;
	float minRatio_identicalPoints = 1. / 2;
	float similarAngleThreshold = M_PI / 16;

protected:
	/// <summary>
	/// Prepares the output.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Calculates the output.
	/// </summary>
	void onExecute() override;

private:
	std::vector<std::vector<std::vector<cv::Point> > > _contours;
	std::vector<std::vector<std::vector<cv::Point> > > _outputContours;
};
} // Buildings
} // AHN
