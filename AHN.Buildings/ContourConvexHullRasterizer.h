#pragma once

#include <string>
#include <vector>

#include <gdal_priv.h>
#include <opencv2/imgproc.hpp>

#include <CloudTools.DEM/DatasetTransformation.hpp>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Converting contours in vector format into a DEM using their convex hulls.
/// </summary>
class ContourConvexHullRasterizer : public CloudTools::DEM::DatasetTransformation<uint32_t, float>
{
public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines computation.
	/// </summary>
	/// <param name="sourceDataset">The reference dataset of the algorithm.</param>
	/// <param name="contours">The input contours for the transformation.</param>
	/// <param name="targetPath">The target file of the algorithm.</param>
	/// <param name="progress">The callback method to report progress.</param>
	ContourConvexHullRasterizer(GDALDataset* sourceDataset,
                                const std::vector<std::vector<cv::Point> >& contours,
                                const std::string& targetPath,
                                CloudTools::Operation::ProgressType progress = nullptr)
		: CloudTools::DEM::DatasetTransformation<uint32_t, float>({ sourceDataset }, targetPath, nullptr, progress), _contours(contours)
	{
		initialize();
	}

	ContourConvexHullRasterizer(const ContourConvexHullRasterizer&) = delete;
	ContourConvexHullRasterizer& operator=(const ContourConvexHullRasterizer&) = delete;

private:
	std::vector<std::vector<cv::Point> > _contours;

	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};
} // Buildings
} // AHN
