#include "Operation.h"

namespace CloudTools
{
namespace DEM
{
#pragma region Operation

void Operation::prepare(bool force)
{
	if (!_isPrepared || force)
	{
		_isPrepared = false;
		_isExecuted = false;
		onPrepare();
		_isPrepared = true;
	}
}

void Operation::execute(bool force)
{
	prepare(force);
	if (!_isExecuted || force)
	{
		_isExecuted = false;
		onExecute();
		_isExecuted = true;
	}
}

#pragma endregion

#pragma region OperationSequence

void OperationSequence::onExecute()
{
	while (!end())
		executeNext();
}

#pragma endregion
} // DEM
} // CloudTools
