#pragma once

#include <iosfwd>
#include <stdexcept>

namespace CloudTools
{
namespace DEM
{
/// <summary>
/// Represents a color in the RGBA space.
/// </summary>
class Color
{
public:
	static const Color Red;
	static const Color Green;
	static const Color Blue;
	static const Color White;
	static const Color Black;
	static const Color Transparent;

private:
	int _red;
	int _green;
	int _blue;
	int _alpha;

public:
	Color() : Color(255, 255, 255) { }
	Color(int red, int green, int blue, int alpha = 255);

	bool operator==(const Color &other) const
	{
		return 
			_red == other._red &&
			_green == other._green &&
			_blue == other._blue &&
			_alpha == other._alpha;
	}
	bool operator!=(const Color &other) const
	{
		return !(*this == other);
	}

	int red() const { return _red; }
	int green() const { return _green; }
	int blue() const { return _blue; }
	int alpha() const { return _alpha; }

	void setRed(int value)
	{
		if (value < 0 || value > 255)
			throw std::invalid_argument("The value should be between 0 and 255.");
		_red = value;
	}

	void setGreen(int value)
	{
		if (value < 0 || value > 255)
			throw std::invalid_argument("The value should be between 0 and 255.");
		_green = value;
	}

	void setBlue(int value)
	{
		if (value < 0 || value > 255)
			throw std::invalid_argument("The value should be between 0 and 255.");
		_blue = value;
	}

	void setAlpha(int value)
	{
		if (value < 0 || value > 255)
			throw std::invalid_argument("The value should be between 0 and 255.");
		_alpha = value;
	}
};

std::istream& operator>>(std::istream &in, Color &color);
} // DEM
} // CloudTools
