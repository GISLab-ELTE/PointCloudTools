#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

#include <ogrsf_frmts.h>

#include "Operation.h"
#include "Metadata.h"

namespace CloudLib
{
namespace DEM
{
/// <summary>
/// Represents a converter of vector layers into a raster filter file.
/// </summary>
class Rasterize : public Operation
{
public:	
	/// <summary>
	/// Output pixel size alongside axis X.
	/// </summary>
	double pixelSizeX = 1;
	/// <summary>
	/// Output pixel size alongside axis Y.
	/// </summary>
	double pixelSizeY = -1;

	/// <summary>
	/// Target output format.
	/// </summary>
	/// <remarks>
	/// For supported formats, <see cref="http://www.gdal.org/formats_list.html" />.
	/// </remarks>
	std::string targetFormat;
	
	/// <summary>
	/// The target value that will be burned.
	/// </summary>
	uint8_t targetValue = 255;

	/// <summary>
	/// Format specific output creation options.
	/// </summary>
	/// <remarks>
	/// For supported options, <see cref="http://www.gdal.org/formats_list.html" />.
	/// </remarks>
	std::map<std::string, std::string> createOptions;

	/// <summary>
	/// The spatial reference system for the source and target files.
	/// </summary>
	/// <remarks>
	/// Setting this property will override SRS detection.
	/// </remarks>
	std::string spatialReference;

	/// <summary>
	/// Callback function for reporting progress.
	/// </summary>
	ProgressType progress;

protected:
	std::string _sourcePath;
	std::string _targetPath;

	GDALDataset* _sourceDataset;
	GDALDataset* _targetDataset;
	std::vector<OGRLayer*> _layers;

	VectorMetadata _sourceMetadata;
	RasterMetadata _targetMetadata;
	bool _isClipped = false;

public:
	/// <summary>
	/// Initializes a new instance of the class and loads source metadata.
	/// </summary>
	/// <remarks>
	/// Loads input metadata.
	/// </remarks>
	/// <param name="sourcePath">The vector source path.</param>
	/// <param name="targetPath">The raster target path.</param>
	/// <param name="layers">The layers to be rasterized. If empty, the first layer will be used.</param>
	/// <param name="progress">The callback method to report progress.</param>
	Rasterize(std::string sourcePath, std::string targetPath,
	          const std::vector<std::string>& layers = std::vector<std::string>(),
	          ProgressType progress = nullptr);
	Rasterize(const Rasterize&) = delete;
	Rasterize& operator=(const Rasterize&) = delete;
	~Rasterize();

	const VectorMetadata& sourceMetadata() const;
	const RasterMetadata& targetMetadata() const;

	/// <summary>
	/// Clips the target raster with a specified window.
	/// </summary>
	/// <param name="originX">The origin X coordinate for the clipping.</param>
	/// <param name="originY">The origin Y coordinate for the clipping.</param>
	/// <param name="rasterSizeX">The raster size alongside axis X for the clipping.</param>
	/// <param name="rasterSizeY">The raster size alongside axis Y for the clipping.</param>
	void clip(double originX, double originY,
	          int rasterSizeX, int rasterSizeY);

protected:
	/// <summary>
	/// Calculates the metadata of the output.
	/// </summary>
	void onPrepare() override;

	/// <summary>
	/// Produces the output file.
	/// </summary>
	void onExecute() override;

private:
	/// <summary>
	/// Routes the C-style GDAL progress reports to the defined reporter.
	/// </summary>
	static int CPL_STDCALL gdalProgress(double dfComplete, const char *pszMessage, void *pProgressArg);
};
} // DEM
} // CloudLib
