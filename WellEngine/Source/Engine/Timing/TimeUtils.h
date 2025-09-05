#pragma once
#include <string>
#include <chrono>

typedef unsigned int UINT;


struct TimeSnapshot
{
	std::string name;
	std::chrono::time_point<std::chrono::high_resolution_clock> snapshot;
};

// Manages total time and delta time as well as capturing snapshots of time for performance testing.
class TimeUtils
{
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> _start;
	std::chrono::time_point<std::chrono::high_resolution_clock> _frame;

	std::vector<TimeSnapshot> _snapshots;

	float _timeScale = 1.0f;

	float _time = 0.0f;
	float _realTime = 0.0f;

	float _deltaTime = 1.0f / 60.0f;
	float _realDeltaTime = 1.0f / 60.0f;
	float _fixedDeltaTime = 1.0f / 20.0f;

public:

	TimeUtils();
	~TimeUtils() = default;
	TimeUtils(const TimeUtils &) = delete;
	TimeUtils &operator=(const TimeUtils &) = delete;
	TimeUtils(TimeUtils &&) = delete;
	TimeUtils &operator=(TimeUtils &&) = delete;

	static inline TimeUtils &GetInstance()
	{
		static TimeUtils instance;
		return instance;
	}

	void Update();

	// Run once to start measuring time, run again with the same name to stop measuring time. Read the time using CompareSnapshots().
	UINT TakeSnapshot(const std::string &name);

	// Read the time between two snapshots using the indexes returned by TakeSnapshot().
	[[nodiscard]] float CompareSnapshots(UINT s1, UINT s2) const;

	// Read the time between two snapshots with the given name.
	[[nodiscard]] float CompareSnapshots(const std::string &name, bool multi = false) const;

	// Try to read the time between two snapshots with the given name. Return true if snapshots were found.
	bool TryCompareSnapshots(const std::string &name, float *time, bool multi = false) const;


	// The difference between in-game timescale and real timescale.
	[[nodiscard]] static float GetTimeScale()
	{
		return GetInstance()._timeScale;
	}

	// In-game time elapsed since start.
	[[nodiscard]] static float GetTime()
	{
		return GetInstance()._time;
	}

	// Real time elapsed since start.
	[[nodiscard]] static float GetRealTime()
	{
		return GetInstance()._realTime;
	}

	// In-game duration of last frame.
	[[nodiscard]] static float GetDeltaTime()
	{
		return GetInstance()._deltaTime;
	}

	// Real duration of last frame.
	[[nodiscard]] static float GetRealDeltaTime()
	{
		return GetInstance()._realDeltaTime;
	}

	// Time between fixed updates.
	[[nodiscard]] static float GetFixedDeltaTime()
	{
		return GetInstance()._fixedDeltaTime;
	}

	static void SetTimeScale(float timeScale)
	{
		GetInstance()._timeScale = timeScale;
	}
	static void SetFixedDeltaTime(float fixedDeltaTime)
	{
		GetInstance()._fixedDeltaTime = fixedDeltaTime;
	}

	TESTABLE()
};
