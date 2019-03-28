#include <cmath>

#include <opencv2/imgproc.hpp>
#include <gdal_priv.h>

#include <CloudTools.DEM/DatasetTransformation.hpp>

using namespace CloudTools::DEM;

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents an edge detection operator using the Canny edge detector
/// https://en.wikipedia.org/wiki/Canny_edge_detector
/// </summary>
template <typename DataType = float>
class CannyEdgeDetector : public DatasetTransformation<GByte, DataType>
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
					  ProgressType progress = nullptr) // TODO: try to use progress properly
		: DatasetTransformation<GByte, DataType>({ sourceDataset }, targetPath, nullptr, progress)
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
				if (hasSourceData(i, j))
				{
					float d = sourceData(i, j);
					if (d < min) min = d;
					if (d > max) max = d;
				}
			}

		// transforming values into 0-255 range
		float interval = max - min;
		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				if (hasSourceData(i, j))
				{
					float value = sourceData(i, j);
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
		cv::Canny(edge, edge,
				  20,
				  20*3, // recommended ratio is 1:3 (or 1:2)
				  3); // equal kernel size with smoothing

		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				uchar isEdge = edge.at<uchar>(j, i);
				setTargetData(i, j, static_cast<GByte>(isEdge > 0 ? 255 : this->nodataValue));
			}
	};
}

} // Buildings
} // AHN
