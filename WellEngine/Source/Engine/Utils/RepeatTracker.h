#pragma once

class RepeatTracker
{
private:
	size_t 
		_lastCount = 0, 
		_currCounter = 0;

public:
	inline void Init() noexcept
	{
		_lastCount = 0;
		_currCounter = 0;
	}

	inline void Step(size_t increase = 1) noexcept
	{
		_currCounter += increase;
	}

	inline void EndFrame() noexcept
	{
		_lastCount = _currCounter;
		_currCounter = 0;
	}

	[[nodiscard]] inline size_t GetCount() const noexcept { return _lastCount; }
	[[nodiscard]] inline size_t GetCurrentCounter() const noexcept { return _currCounter; }
};
