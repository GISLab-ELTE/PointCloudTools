#pragma once

#include <string>
#include <vector>

#include <gdal_priv.h>
#include <opencv2/imgproc.hpp>

#include <CloudTools.DEM/DatasetCalculation.hpp>


namespace AHN
{
namespace Buildings
{
/// <summary>
/// Performs countour detection on a DEM dataset based on the Canny edge detector.
/// https://en.wikipedia.org/wiki/Canny_edge_detector
/// </summary>
class ContourDetection : public CloudTools::DEM::DatasetCalculation<float>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourcePath">The source file of the transformation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	ContourDetection(const std::string& sourcePath,
					 Operation::ProgressType progress = nullptr)
		: DatasetCalculation<float>({ sourcePath }, nullptr, progress)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the transformation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	ContourDetection(GDALDataset* sourceDataset,
					 Operation::ProgressType progress = nullptr)
		: DatasetCalculation<float>({ sourceDataset }, nullptr, progress)
	{
		initialize();
	}

	ContourDetection(const ContourDetection&) = delete;
	ContourDetection& operator=(const ContourDetection&) = delete;

	/// <summary>
	/// Retrieves the detected contours.
	/// </summary>
	/// <returns>The detected contours.</returns>
	std::vector<std::vector<cv::Point> >& getContours();

	int smoothingFilterKernelSize = 3;
	int cannyUpperThreshold;
	int cannyThresholdRatio = 3;

private:
	std::vector<std::vector<cv::Point> > _contours;

	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};
} // Buildings
} // AHN
