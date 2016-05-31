#include <cstring>

#include "Reporter.h"
#include "IO.h"

namespace CloudTools
{
namespace IO
{
#pragma region TextReporter

void TextReporter::report(float complete, const std::string &message)
{
	eraseLine(_eraseSize);
	_eraseSize = std::strlen("Progress: 00.00% ()") + message.length();
	reportProgress(complete, message);
}

void TextReporter::reset()
{
	eraseLine(_eraseSize);
	_eraseSize = 0;
}

#pragma endregion 

#pragma region BarReporter

BarReporter::~BarReporter()
{
	if (_progress)
		delete _progress;
}

void BarReporter::report(float complete, const std::string &message)
{
	if (!_progress) _progress = new boost::progress_display(_size);
	report(static_cast<unsigned int>(_progress->expected_count() * complete));
}

void BarReporter::report(unsigned int complete, const std::string &message)
{
	if (!_progress) _progress = new boost::progress_display(_size);
	*_progress += (complete - _progress->count());
}

void BarReporter::reset()
{
	if (_progress)
		_progress->restart(_size);
}

#pragma endregion
} // IO
} // CloudTools
