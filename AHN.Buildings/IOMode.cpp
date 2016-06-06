#include <string>

#include <boost/algorithm/string.hpp>

#include "IOMode.h"

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
	else if (str == "STREAMING")
		mode = IOMode::Streaming;
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
	case IOMode::Streaming:
		output << "STREAMING";
		break;
	case IOMode::Hadoop:
		output << "HADOOP";
		break;
	default:
		output << "UNKNOWN";
	}
	return output;
}
