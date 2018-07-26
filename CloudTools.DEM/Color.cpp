#include <string>
#include <algorithm>
#include <vector>
#include <istream>

#include <boost/algorithm/string.hpp>

#include "Color.h"

namespace CloudTools
{
namespace DEM
{
Color::Color(int red, int green, int blue, int alpha)
{
	setRed(red);
	setGreen(green);
	setBlue(blue);
	setAlpha(alpha);
}

const Color Color::Red(255, 0, 0);
const Color Color::Green(0, 255, 0);
const Color Color::Blue(0, 0, 255);
const Color Color::White(255, 255, 255);
const Color Color::Black(0, 0, 0);
const Color Color::Transparent(0, 0, 0, 0);

std::istream& operator>>(std::istream &in, Color &color)
{
	std::string input;
	in >> input;
	std::transform(input.begin(), input.end(), input.begin(), ::tolower);

	if (input == "red")
		color = Color::Red;
	else if (input == "green")
		color = Color::Green;
	else if (input == "blue")
		color = Color::Blue;
	else if (input == "white")
		color = Color::White;
	else if (input == "black")
		color = Color::Black;
	else if (input == "transparent")
		color = Color::Transparent;
	else
	{
		std::vector<std::string> params;
		boost::split(params, input, boost::is_any_of(","));
		if (params.size() < 3)
			throw std::invalid_argument("At least 3 bands must given for an RGBA color.");
		if (params.size() > 4)
			throw std::invalid_argument("Maximum 4 bands can given for an RGBA color.");

		try
		{
			color.setRed(std::stoi(params[0]));
			color.setGreen(std::stoi(params[1]));
			color.setBlue(std::stoi(params[2]));
			if (params.size() == 4)
				color.setAlpha(std::stoi(params[3]));
			else
				color.setAlpha(255);
		}
		catch (std::invalid_argument&)
		{
			throw std::invalid_argument("Bad value for color band, values must be between 0-255.");
		}
	}
	return in;
}
} // DEM
} // CloudTools
