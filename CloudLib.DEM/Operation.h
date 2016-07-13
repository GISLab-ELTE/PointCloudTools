#pragma once

#include <string>
#include <functional>

namespace CloudLib
{
namespace DEM
{
/// <summary>
/// Represents an operation on DEM datasets.
/// </summary>
class Operation
{
public:
	typedef std::function<bool(float, const std::string&)> ProgressType;

private:
	bool _isPrepared = false;
	bool _isExecuted = false;

public:
	virtual ~Operation() { }
	
	/// <summary>
	/// Determines whether the operation is prepared.
	/// </summary>
	/// <returns><c>true</c> if the operation has been already prepared; otherwise <c>false</c>.</returns>
	bool isPrepared() const { return _isPrepared; }
	
	/// <summary>
	/// Determines whether the operation is executed.
	/// </summary>
	/// <returns><c>true</c> if the operation has been already executed; otherwise <c>false</c>.</returns>
	bool isExecuted() const { return _isExecuted; }
	
	/// <summary>
	/// Prepares the operation.
	/// </summary>
	/// <param name="force">Forces reevaluation if necessary.</param>
	void prepare(bool force = false);
	
	/// <summary>
	/// Executes the operation.
	/// </summary>
	/// <param name="force">Forces reevaluation if necessary.</param>
	void execute(bool force = false);

protected:
	/// <summary>
	/// Verifies input data and prepares the output.
	/// </summary>
	virtual void onPrepare() = 0;

	/// <summary>
	/// Produces the output data.
	/// </summary>
	virtual void onExecute() = 0;
};

/// <summary>
/// Represents an iterative operation sequence on DEM datasets.
/// </summary>
class OperationSequence : public Operation
{
public:
	/// <summary>
	/// Prepares the next operation.
	/// </summary>
	/// <param name="force">Forces reevaluation if necessary.</param>
	virtual void prepareNext(bool force = false) = 0;

	/// <summary>
	/// Executes the next operation.
	/// </summary>
	/// <param name="force">Forces reevaluation if necessary.</param>
	virtual void executeNext(bool force = false) = 0;

	/// <summary>
	/// Determines whether all steps of the computation has been evaluated.
	/// </summary>
	virtual bool end() const = 0;

protected:
	void onPrepare() override final { }
	void onExecute() override final;
};
} // DEM
} // CloudLib
