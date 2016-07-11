#include "ResultFile.h"

#include <boost/algorithm/string/predicate.hpp>

namespace AHN
{
ResultFile::~ResultFile()
{
	if (dataset != nullptr)
		GDALClose(dataset);
	if (isPhysical() && !isPermanent)
		fs::remove(path);
	else if (isVirtual())
		VSIUnlink(path.string().c_str());
}

ResultFile::ResultFile(ResultFile&& other)
{
	path = other.path;
	dataset = other.dataset;
	isPermanent = other.isPermanent;

	other.path.clear();
	other.dataset = nullptr;
}

ResultFile& ResultFile::operator=(ResultFile&& other)
{
	if (this == &other)
		return *this;

	path = other.path;
	dataset = other.dataset;
	isPermanent = other.isPermanent;

	other.path.clear();
	other.dataset = nullptr;
	return *this;
}

bool ResultFile::isPhysical() const
{
	return !path.empty() && !boost::starts_with(path.string(), "/vsimem/");
}

bool ResultFile::isVirtual() const
{
	return boost::starts_with(path.string(), "/vsimem/");
}

bool ResultFile::isInMemory() const
{
	return path.empty();
}
} // AHN
