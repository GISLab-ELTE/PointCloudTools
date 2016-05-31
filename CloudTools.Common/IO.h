#pragma once

#include <string>
#include <functional>

namespace CloudTools
{
namespace IO
{
#pragma region Types

enum ExitCodes
{
	// Success
	Success = 0,
	UserAbort = -1,
	NoResult = -2,

	// Errors
	InvalidInput = 1,
	UnexcpectedError = 2,
	Unsupported = 3,
};

#pragma endregion

#pragma region Input operations

/// <summary>
/// Reads a boolean value from the console input.
/// </summary>
/// <param name="msg">The message to display.</param>
/// <param name="def">The default return value.</param>
bool readBoolean(const std::string &msg, bool def = true);

#pragma endregion 

#pragma region Output operations

/// <summary>
/// Displays the progress of a computation.
/// </summary>
/// <param name="complete">The ratio of completeness of the process from 0.0 for just started to 1.0 for completed.</param>
/// <param name="message">An optional message to display.</param>
void reportProgress(float complete, const std::string &message = std::string());

/// <summary>
/// Erases the current line on console.
/// </summary>
/// <param name="complete">The number of characters to erase.</param>
void eraseLine(unsigned int size = 32);

#pragma endregion 
} // IO
} // CloudTools
