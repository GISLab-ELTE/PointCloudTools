#include "ResultCollection.h"

namespace CloudTools
{
namespace IO
{
ResultCollection::~ResultCollection()
{
	for (auto& item : _results)
		delete item.second;
	_results.clear();
}

Result& ResultCollection::result(const std::string& name, std::size_t index)
{
	if (_results.count(name) <= index)
		throw std::out_of_range("No result found with the given name and index.");

	auto range = _results.equal_range(name);
	auto it = range.first;
	std::advance(it, index);
	return *it->second;
}

std::size_t ResultCollection::newResult(const std::string& name, bool isFinal)
{
	std::pair<std::string, Result*> item(name, createResult(name, isFinal));
	_results.emplace(std::move(item));
	return _results.count(name) - 1;
}

void ResultCollection::deleteResult(const std::string& name, std::size_t index)
{
	if (_results.count(name) <= index)
		throw std::out_of_range("No result found with the given name and index.");

	auto range = _results.equal_range(name);
	auto it = range.first;
	std::advance(it, index);
	delete it->second;
	_results.erase(it);
}
} // IO
} // CloudTools