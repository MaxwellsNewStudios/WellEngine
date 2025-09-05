#pragma once

class Identifiable
{
private:
	static const size_t GetNextUID() noexcept
	{
		static size_t _topUID = 0ull;
		return _topUID++;
	}
	const size_t _uid = GetNextUID();

public:
	Identifiable() = default;
	~Identifiable() = default;
	Identifiable(const Identifiable &) = delete;
	Identifiable &operator=(const Identifiable &) = delete;
	Identifiable(Identifiable &&) noexcept = delete;
	Identifiable &operator=(Identifiable &&) noexcept = delete;

	[[nodiscard]] const size_t &GetUID() const noexcept
	{
		return _uid;
	}

	TESTABLE()
};