#include "stdafx.h"
#include "Timing/TimeUtils.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

TimeUtils::TimeUtils() : _start(std::chrono::high_resolution_clock::now()), _frame(std::chrono::high_resolution_clock::now())
{

}

void TimeUtils::Update()
{
	const auto newFrame = std::chrono::high_resolution_clock::now();

	const std::chrono::duration<float> newTime = newFrame - _start;
	_realTime = newTime.count();

	const std::chrono::duration<float> newDeltaTime = newFrame - _frame;
	_realDeltaTime = newDeltaTime.count();
	_deltaTime = _realDeltaTime * _timeScale;
	_time += _deltaTime;

	_snapshots.clear();
	_frame = newFrame;
}

UINT TimeUtils::TakeSnapshot(const std::string &name)
{
	_snapshots.emplace_back(name, std::chrono::high_resolution_clock::now());
	return static_cast<UINT>(_snapshots.size() - 1);
}
float TimeUtils::CompareSnapshots(const UINT s1, const UINT s2) const
{
	if (s1 >= s2)
		return -1.0f;

	if (s2 >= static_cast<UINT>(_snapshots.size()))
		return -1.0f;

	const std::chrono::duration<float> duration = _snapshots[s2].snapshot - _snapshots[s1].snapshot;
	return duration.count();
}
float TimeUtils::CompareSnapshots(const std::string &name, bool multi) const
{
	UINT s1 = 0, s2 = 0;
	bool foundFirst = false;

	if (multi)
	{
		float cumulativeTime = 0.0f;

		for (UINT i = 0; i < static_cast<UINT>(_snapshots.size()); i++)
		{
			if (_snapshots[i].name != name)
				continue;

			if (foundFirst)
			{
				s2 = i;
				cumulativeTime += CompareSnapshots(s1, s2);

				// Reset for next pair
				foundFirst = false; 
				s1 = s2 = 0;
				continue;
			}

			s1 = i;
			foundFirst = true;
		}

		if (foundFirst)
		{
			// If we found an odd number of snapshots, we need to add the last one
			cumulativeTime += CompareSnapshots(s1, static_cast<UINT>(_snapshots.size()));
		}

		return cumulativeTime;
	}
	else
	{
		for (UINT i = 0; i < static_cast<UINT>(_snapshots.size()); i++)
		{
			if (_snapshots[i].name != name)
				continue;

			if (foundFirst)
			{
				s2 = i;
				break;
			}

			s1 = i;
			foundFirst = true;
		}

		return CompareSnapshots(s1, s2);
	}
}
bool TimeUtils::TryCompareSnapshots(const std::string &name, float *time, bool multi) const
{
	UINT s1 = 0, s2 = 0;
	bool foundFirst = false;

	if (multi)
	{
		bool foundAPair = false;

		for (UINT i = 0; i < static_cast<UINT>(_snapshots.size()); i++)
		{
			if (_snapshots[i].name != name)
				continue;

			if (foundFirst)
			{
				s2 = i;
				*time += CompareSnapshots(s1, s2);
				foundAPair = true;
				
				// Reset for next pair
				foundFirst = false;
				s1 = s2 = 0;
				continue;
			}

			s1 = i;
			foundFirst = true;
		}

		if (foundFirst)
		{
			// If we found an odd number of snapshots, the comparison failed
			return false;
		}
		
		return foundAPair;
	}
	else
	{
		bool foundSecond = false;

		for (UINT i = 0; i < static_cast<UINT>(_snapshots.size()); i++)
		{
			if (_snapshots[i].name != name)
				continue;

			if (foundFirst)
			{
				foundSecond = true;
				s2 = i;
				break;
			}

			s1 = i;
			foundFirst = true;
		}

		if (!foundSecond)
			return false;

		*time = CompareSnapshots(s1, s2);
	}
	
	return true;
}
