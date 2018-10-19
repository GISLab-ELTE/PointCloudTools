#pragma once

#include <string>

#include <boost/progress.hpp>

namespace CloudTools
{
namespace IO
{
/// <summary>
/// Represents an abstract progress reporter.
/// </summary>
class Reporter
{
public:
	virtual ~Reporter() {}
	
	/// <summary>
	/// Displays the progress of a computation.
	/// </summary>
	/// <param name="complete">The ratio of completeness of the process from 0.0 for just started to 1.0 for completed.</param>
	/// <param name="message">An optional message string to display.</param>
	virtual void report(float complete, const std::string &message = std::string()) = 0;

	/// <summary>
	/// Resets the progress.
	/// </summary>
	virtual void reset() = 0;
};

/// <summary>
/// Represents a textual numerical progress reporter.
/// </summary>
class TextReporter : public Reporter
{
	std::size_t  _eraseSize = 0;

public:			
	/// <summary>
	/// Displays the progress of a computation.
	/// </summary>
	/// <param name="complete">The ratio of completeness of the process from 0.0 for just started to 1.0 for completed.</param>
	/// <param name="message">An optional message string to display.</param>
	void report(float complete, const std::string &message = std::string()) override;

	/// <summary>
	/// Resets the progress.
	/// </summary>
	void reset() override;
};

/// <summary>
/// Represents an ASCII progress bar reporter.
/// </summary>
class BarReporter : public Reporter
{
	boost::progress_display *_progress;
	unsigned int _size;

public:
	explicit BarReporter(unsigned int size = 100) 
		: _size(size), _progress(nullptr) { }
	~BarReporter();

	/// <summary>
	/// Displays the progress of a computation.
	/// </summary>
	/// <param name="complete">The ratio of completeness of the process from 0.0 for just started to 1.0 for completed.</param>
	/// <param name="message">An optional message string to display.</param>
	void report(float complete, const std::string &message = std::string()) override;

	/// <summary>
	/// Displays the progress of a computation.
	/// </summary>
	/// <param name="complete">The number of completed elements, where the total number of elements where given in the constrcutor.</param>
	/// <param name="message">An optional message string to display.</param>
	void report(unsigned int complete, const std::string &message = std::string());

	/// <summary>
	/// Resets the progress.
	/// </summary>
	void reset() override;
};

/// <summary>
/// Represents a mute progress reporter which outputs nothing.
/// </summary>
class NullReporter : public Reporter
{
	int _eraseSize = 0;

public:
	/// <summary>
	/// Displays the progress of a computation.
	/// </summary>
	/// <param name="complete">The ratio of completeness of the process from 0.0 for just started to 1.0 for completed.</param>
	/// <param name="message">An optional message string to display.</param>
	void report(float complete, const std::string &message = std::string()) override { }

	/// <summary>
	/// Resets the progress.
	/// </summary>
	void reset() override { }
};
} // IO
} // CloudTools
