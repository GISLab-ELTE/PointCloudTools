#pragma once

#include <iostream>
#include "../SweepLineTransformation.hpp"

namespace CloudTools
{
namespace DEM
{
class MatrixTransformation : public SweepLineTransformation<float>
{
private:
	float* _matrix;

public:
	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourcePath">The source file of the matrix transformation.</param>
	/// <param name="targetPath">The target file of the matrix transformation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MatrixTransformation(std::string sourcePath,
	                     const std::string& targetPath,
	                     int range,
	                     Operation::ProgressType progress = nullptr)
		: SweepLineTransformation<float>({ sourcePath }, targetPath, range, nullptr, progress)
	{
		initialize();
	}

	/// <summary>
	/// Initializes a new instance of the class. Loads input metadata and defines calculation.
	/// </summary>
	/// <param name="sourceDataset">The source dataset of the matrix transformation.</param>
	/// <param name="targetPath">The target file of the  matrix transformation.</param>
	/// <param name="progress">The callback method to report progress.</param>
	MatrixTransformation(GDALDataset* sourceDataset,
	                     const std::string& targetPath,
	                     int range,
	                     ProgressType progress = nullptr)
		: SweepLineTransformation<float>({ sourceDataset }, targetPath, range, nullptr, progress)
	{
		initialize();
	}

	MatrixTransformation(const MatrixTransformation&) = delete;
	MatrixTransformation& operator=(const MatrixTransformation&) = delete;

	float getMatrix(int i, int j) const
	{
		if (i < -_range || i > _range)
			throw std::out_of_range("i is out of range.");
		if (j < -_range || j > _range)
			throw std::out_of_range("j is out of range.");

		return _matrix[(_range + i) * _range + (_range + j)];
	}

	void setMatrix(int i, int j, float value)
	{
		if (i < -_range || i > _range)
			throw std::out_of_range("i is out of range.");
		if (j < -_range || j > _range)
			throw std::out_of_range("j is out of range.");

		_matrix[(_range + i) * _range + (_range + j)] = value;
	}

private:
	void initialize();
};
}
}
