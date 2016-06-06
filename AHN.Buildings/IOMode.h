#pragma once

#include <iosfwd>

enum class IOMode
{
	Unknown   =  0, // 0000
	Files     =  1, // 0001
	Memory    =  2, // 0010
	Streaming =  6, // 0110
	Hadoop    = 14, // 1110
};
using Internal = std::underlying_type<IOMode>::type;

inline IOMode operator | (IOMode lhs, IOMode rhs)
{
	return static_cast<IOMode>(static_cast<Internal>(lhs) | static_cast<Internal>(rhs));
}

inline IOMode operator &(IOMode lhs, IOMode rhs)
{
	return static_cast<IOMode>(static_cast<Internal>(lhs) & static_cast<Internal>(rhs));
}

inline bool isValid(IOMode mode)
{
	return mode != IOMode::Unknown;
}

std::istream& operator >> (std::istream& input, IOMode &mode);
std::ostream& operator<< (std::ostream& output, const IOMode &mode);
