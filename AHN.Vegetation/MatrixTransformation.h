#pragma once

#include <CloudTools.DEM/SweepLineTransformation.h>


namespace AHN
{
namespace Vegetation
{

class MatrixTransformation : public CloudTools::DEM::SweepLineTransformation<float>
{
public:
	MatrixTransformation(GDALDataset *sourceDataset,
		const std::string& targetPath,
		int range,
		ProgressType progress = nullptr);
	MatrixTransformation(const MatrixTransformation&) = delete;
	MatrixTransformation& operator=(const MatrixTransformation&) = delete;

private:
	float center, middle, corner;
};
}
}