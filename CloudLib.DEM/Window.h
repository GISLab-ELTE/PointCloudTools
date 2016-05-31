#pragma once

#include <stdexcept>

namespace CloudLib
{
namespace DEM
{
/// <summary>
/// Represents a window to a sub-dataset matrix with continuous representation.
/// </summary>
template <typename DataType>
struct Window
{
public:
	int centerX;
	int centerY;

private:
	const DataType* _data;
	const DataType _nodataValue;

	const int _sizeX;
	const int _sizeY;
	const int _offsetX;
	const int _offsetY;

public:	
	/// <summary>
	/// Initializes a new instance of the struct.
	/// </summary>
	/// <param name="data">The continuous data.</param>
	/// <param name="nodataValue">The nodata value.</param>
	/// <param name="sizeX">The width of the matrix.</param>
	/// <param name="sizeY">The height of the matrix.</param>
	/// <param name="offsetX">The abcissa offset of the sub-dataset.</param>
	/// <param name="offsetY">The ordinate offset of the sub-dataset.</param>
	/// <param name="centerX">The center abcissa position of inquiry.</param>
	/// <param name="centerY">The center ordinate position of inquiry.</param>
	Window(const DataType* data, DataType nodataValue,
	       int sizeX, int sizeY,
	       int offsetX, int offsetY,
	       int centerX, int centerY)
		: _data(data), _nodataValue(nodataValue),
		  _sizeX(sizeX), _sizeY(sizeY),
		  _offsetX(offsetX), _offsetY(offsetY),
		  centerX(centerX), centerY(centerY)
	{
		if (_sizeX < 0 || _sizeY < 0)
			throw std::invalid_argument("Negative data dimensions are prohibited.");
	}

	/// <summary>
	/// Determines whether the given center contains a valid data in the sub-dataset.
	/// </summary>
	bool hasData() const
	{
		return hasData(0, 0);
	}
	
	/// <summary>
	/// Determines whether the specified position relative to the given center contains a valid data in the sub-dataset.
	/// </summary>
	/// <param name="i">Relative abcissa.</param>
	/// <param name="j">Relative ordinate.</param>
	bool hasData(int i, int j) const
	{
		if (!isValid(i, j))
			return false;
		return _data[position(i, j)] != _nodataValue;
	}

	/// <summary>
	/// Retrieves the data at the given center in the sub-dataset.
	/// </summary>
	DataType data() const
	{
		return data(0, 0);
	}

	/// <summary>
	/// Retrieves the data at the specified position relative to the given center in the sub-dataset.
	/// </summary>
	/// <param name="i">Relative abcissa.</param>
	/// <param name="j">Relative ordinate.</param>
	DataType data(int i, int j) const
	{
		if (!isValid(i, j))
			return _nodataValue;
		return _data[position(i, j)];
	}

private:
	bool isValid(int i, int j) const
	{
		return centerX + i >= _offsetX && centerX + i < _offsetX + _sizeX &&
			   centerY + j >= _offsetY && centerY + j < _offsetY + _sizeY;
	}

	int position(int i, int j) const
	{
		return (centerY - _offsetY + j) * _sizeX + centerX - _offsetX + i;
	}
};
} // DEM
} // CloudLib

