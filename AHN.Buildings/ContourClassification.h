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
/// Distinguishing the building contours from any other.
/// </summary>
class ContourClassification: public CloudTools::Operation
{
public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="contours">The input contours.</param>
	ContourClassification(const std::vector<std::vector<std::vector<cv::Point> > >& contours) : _contours(contours)
	{ }

	/// <summary>
	/// Retrieves the output contours of the operation.
	/// </summary>
	/// <returns>The contours denoting buildings.</returns>
	std::vector<std::vector<cv::Point> >& getContours();

	int closurePixelDifference = 5;
	float similarAngleThreshold = M_PI / 18;
	int chainingPixelDifference = 15;
	int minBuildingPixelPerimeter = 80;
	int contourPartitionNumber = 8;
	int longSegmentPixelThreshold = 15;
	float condition1Ratio = 3. / 4;
	float condition2Ratio = 1. / 4;
	float condition3Ratio = 1. / 4;
	float avgSegmentPixels = 15;
	float minChainingRatio = 3. / 4;
	int maxChainingPixelError = 20;

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
	std::vector<std::vector<cv::Point> > _buildingContours;
};
} // Buildings
} // AHN
