#include <stdexcept>

#include <boost/filesystem.hpp>

#include "Transformation.h"

namespace fs = boost::filesystem;

namespace CloudLib
{
namespace DEM
{
Transformation::~Transformation()
{
	if (_targetOwnerShip && _targetDataset != nullptr)
		GDALClose(_targetDataset);
}

GDALDataset* Transformation::target()
{
	if (!isExecuted())
		throw std::logic_error("The computation is not executed.");
	_targetOwnerShip = false;
	return _targetDataset;
}
} // DEM
} // CloudLib
