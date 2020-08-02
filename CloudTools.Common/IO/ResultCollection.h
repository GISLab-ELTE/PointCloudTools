#pragma once

#include <string>
#include <unordered_map>

#include "Result.h"

namespace CloudTools
{
namespace IO
{
class ResultCollection
{
private:
	std::multimap<std::string, CloudTools::IO::Result*> _results;

public:
	virtual ~ResultCollection();

protected:
	/// <summary>
	/// Gets the specified result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="index">The index of the result with the same name.</param>
	CloudTools::IO::Result& result(const std::string& name, std::size_t index = 0);

	/// <summary>
	/// Creates and inserts new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>The index of the result with the same name.</returns>
	std::size_t newResult(const std::string& name, bool isFinal = false);

	/// <summary>
	/// Creates a new result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="isFinal"><c>true</c> if the result is final, otherwise <c>false</c>.</param>
	/// <returns>New result on the heap.</returns>
	virtual CloudTools::IO::Result* createResult(const std::string& name, bool isFinal = false) = 0;

	/// <summary>
	/// Deletes the specified result object.
	/// </summary>
	/// <param name="name">The name of the result.</param>
	/// <param name="index">The index of the result with the same name.</param>
	void deleteResult(const std::string& name, std::size_t index = 0);
};
} // IO
} // CloudTools
