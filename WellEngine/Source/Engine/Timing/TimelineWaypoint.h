#pragma once
#include <DirectXMath.h>

class TimelineWaypoint
{
private:
	dx::XMFLOAT3A _worldPosition = { 0, 0, 0 };
	dx::XMFLOAT4A _worldRotation = { 0, 0, 0, 1 };
	dx::XMFLOAT3A _worldScale = { 1, 1, 1 };

	float _time = 0.0f;

public:
	TimelineWaypoint() = default;
	TimelineWaypoint(
		dx::XMFLOAT3A worldPosition,
		dx::XMFLOAT4A worldRotation,
		dx::XMFLOAT3A worldScale,
		const float time
	): _worldPosition(worldPosition), _worldRotation(worldRotation), _worldScale(worldScale), _time(time) {}

	~TimelineWaypoint() = default;

	TimelineWaypoint(const TimelineWaypoint &other) = default;
	TimelineWaypoint& operator=(const TimelineWaypoint &other) = default;
	TimelineWaypoint(TimelineWaypoint &&other) = default;
	TimelineWaypoint& operator=(TimelineWaypoint &&other) = default;

	[[nodiscard]] const dx::XMFLOAT3A &GetPosition() const;
	[[nodiscard]] const dx::XMFLOAT4A &GetRotation() const;
	[[nodiscard]] const dx::XMFLOAT3A &GetScale() const;
	[[nodiscard]] const float &GetTime() const;

	void SetTime(const float time);
	bool Serialize(std::string *code) const;
	bool Deserialize(const std::string &code);

#ifdef USE_IMGUI
	bool RenderUI();
#endif

	TESTABLE()
};