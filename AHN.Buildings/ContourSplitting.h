#pragma once

#include <vector>

#include <opencv2/imgproc.hpp>

#include <CloudTools.Common/Operation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Dividing contours into straight segments.
/// </summary>
class ContourSplitting: public CloudTools::Operation
{
public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="contours">The input contours.</param>
	ContourSplitting(const std::vector<std::vector<cv::Point> >& contours) : _contours(contours)
	{ }

	/// <summary>
	/// Retrieves the output contours of the operation.
	/// </summary>
	/// <returns>The straight contour segments for each initial contour.</returns>
	std::vector<std::vector<std::vector<cv::Point> > >& getContours();

	int minStraightSegmentLength = 5;

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
	std::vector<std::vector<cv::Point> > _contours;
	std::vector<std::vector<std::vector<cv::Point> > > _segmentedContours;
};
} // Buildings
} // AHN
