#include "../Window.hpp"
#include "MatrixTransformation.h"

using namespace CloudTools::DEM;

namespace CloudTools
{
namespace DEM
{
void MatrixTransformation::initialize()
{
	// Initialize convolution matrix
	const int matrixSize = 2 * _range + 1;
	this->_matrix = new float[matrixSize * matrixSize];
	std::fill(this->_matrix, this->_matrix + matrixSize * matrixSize, 1.f);

	// Set computation method
	this->computation = [this](int x, int y, const std::vector<Window<float>>& sources)
	{
		const Window<float>& source = sources[0];
		if (!source.hasData()) return static_cast<float>(this->nodataValue);

		float value = 0;
		int counter = 0;
		for(int i = -this->range(); i <= this->range(); ++i)
			for(int j = -this->range(); j <= this->range(); ++j)
				if (source.hasData(i, j))
				{
					const float matrixValue = this->getMatrix(i, j);
					value += (source.data(i, j) * matrixValue);
					counter += matrixValue;
				}

		return value / counter;
	};
	this->nodataValue = 0;
}
}
}