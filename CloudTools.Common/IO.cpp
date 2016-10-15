#include <cstring>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include "IO.h"

namespace CloudTools
{
namespace IO
{
#pragma region Input operations

bool readBoolean(const std::string &msg, bool def)
{
	std::cout << msg;
	if (def)
		std::cout << " [Y/n] " << std::flush;
	else
		std::cout << " [y/N] " << std::flush;

	char answer[4];
	std::cin.getline(answer, 4);
	if (strlen(answer) == 0) return def;

	std::transform(answer, answer + strlen(answer), answer, ::tolower);
	return strcmp(answer, "yes") == 0 || strcmp(answer, "y") == 0;
}

#pragma endregion 

#pragma region Output operations

void reportProgress(float complete, const std::string &message)
{
	eraseLine();
	std::cout
		<< "\rProgress: "
		<< std::fixed << std::setprecision(2) << (complete * 100) << "%" 
		<< std::flush;
	if (message.length() > 0)
		std::cout << " (" << message << ")" << std::flush;
	if (complete == 1.0)
		std::cout << std::endl;
}

void eraseLine(std::size_t size)
{
	for (unsigned int i = 0; i < size; ++i)
		std::cout << '\b';
	std::cout << '\r' << std::flush;
}

#pragma endregion 
} // IO
} // CloudTools
