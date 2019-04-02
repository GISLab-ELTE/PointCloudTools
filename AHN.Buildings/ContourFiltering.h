#pragma once

#include <vector>

#include <opencv2/imgproc.hpp>

#include <CloudTools.Common/Operation.h>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Initial filtering for building contours examining size and shape.
/// </summary>
class ContourFiltering : public CloudTools::Operation
{
public:
	/// <summary>
	/// Initializes a new instance of the class.
	/// </summary>
	/// <param name="contours">The input contours to be filtered.</param>
	ContourFiltering(const std::vector<std::vector<cv::Point> >& contours) : _contours(contours)
	{ }

	/// <summary>
	/// Retrieves the output contours of the operation.
	/// </summary>
	/// <returns>The remaining contours.</returns>
	std::vector<std::vector<cv::Point> >& getContours();

	int subsequenceLength_monotonic = 20;
	int subsequenceLength_straight = 40;
	float minRatio_monotonic = 5. / 6;

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
	std::vector<std::vector<cv::Point> > _filteredContours;
};
} // Buildings
} // AHN
