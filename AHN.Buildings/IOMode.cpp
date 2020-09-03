#include <string>
#include <istream>

#include <boost/algorithm/string.hpp>

#include "IOMode.h"

namespace AHN
{
namespace Buildings
{
std::istream& operator >> (std::istream& input, IOMode &mode)
{
	std::string str;
	input >> str;
	boost::to_upper(str);
	boost::trim(str);

	if (str == "FILES")
		mode = IOMode::Files;
	else if (str == "MEMORY")
		mode = IOMode::Memory;
	else if (str == "STREAM")
		mode = IOMode::Stream;
	else if (str == "HADOOP")
		mode = IOMode::Hadoop;
	else
		mode = IOMode::Unknown;
	return input;
}

std::ostream& operator<< (std::ostream& output, const IOMode &mode)
{
	switch (mode)
	{
	case IOMode::Files:
		output << "FILES";
		break;
	case IOMode::Memory:
		output << "MEMORY";
		break;
	case IOMode::Stream:
		output << "STREAM";
		break;
	case IOMode::Hadoop:
		output << "HADOOP";
		break;
	default:
		output << "UNKNOWN";
	}
	return output;
}
} // Buildings
} // AHN
