#pragma once

typedef size_t UID;

class Identifiable
{
private:
	static const UID GetNextUID() noexcept
	{
		static UID _topUID = 0ull;
		return _topUID++;
	}
	const UID _uid = GetNextUID();

public:
	Identifiable() = default;
	~Identifiable() = default;
	Identifiable(const Identifiable &) = delete;
	Identifiable &operator=(const Identifiable &) = delete;
	Identifiable(Identifiable &&) noexcept = delete;
	Identifiable &operator=(Identifiable &&) noexcept = delete;

	[[nodiscard]] const UID &GetUID() const noexcept
	{
		return _uid;
	}

	TESTABLE()
};
