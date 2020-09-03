#include <stdexcept>

#include "Creation.h"

namespace CloudTools
{
namespace DEM
{
Creation::~Creation()
{
	if (_targetOwnerShip && _targetDataset != nullptr)
		GDALClose(_targetDataset);
}

GDALDataset* Creation::target()
{
	if (!isExecuted())
		throw std::logic_error("The computation is not executed.");
	_targetOwnerShip = false;
	return _targetDataset;
}
} // DEM
} // CloudTools
