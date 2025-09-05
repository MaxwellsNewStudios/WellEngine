#include "stdafx.h"
#include "Timing/TimelineSequence.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

void TimelineSequence::Lerp(Transform* transform, float t, TimelineWaypoint* start, TimelineWaypoint* end)
{
	float factor = 0;
	dx::XMFLOAT3A startPos;
	dx::XMVECTOR startQuat;
	dx::XMFLOAT3A startScale;

	dx::XMFLOAT3A endPos = end->GetPosition();
	dx::XMVECTOR endQuat = dx::XMLoadFloat4A(&end->GetRotation());
	dx::XMFLOAT3A endScale = end->GetScale();


	// If there is no start waypoint, lerp from the start position, rotation and scale.
	if (start == nullptr)
	{
		float endTime = end->GetTime();
		endTime = endTime == 0 ? _lerpToStartTime : endTime;
		
		factor = t / endTime;

		startPos = _startPosition;
		startQuat = dx::XMLoadFloat4A(&_startRotation);
		startScale = _startScale;
	}
	// Otherwise, calculate the interpolation factor and get the start transforms.
	else
	{
		// Calculate the interpolation factor.
		factor = (t - start->GetTime()) / (end->GetTime() - start->GetTime());

		startPos = start->GetPosition();
		startQuat = dx::XMLoadFloat4A(&start->GetRotation());
		startScale = start->GetScale();
	}

	switch (_easing)
	{
	case Func_None:													break;
	case Func_Linear:												break;
	case Func_EaseInSine:		factor = EaseInSine(factor);		break;
	case Func_EaseOutSine:		factor = EaseOutSine(factor);		break;
	case Func_EaseInOutSine:	factor = EaseInOutSine(factor);		break;
	case Func_EaseInQuad:		factor = EaseInQuad(factor);		break;
	case Func_EaseOutQuad:		factor = EaseOutQuad(factor);		break;
	case Func_EaseInOutQuad:	factor = EaseInOutQuad(factor);		break;
	case Func_EaseInCubic:		factor = EaseInCubic(factor);		break;
	case Func_EaseOutCubic:		factor = EaseOutCubic(factor);		break;
	case Func_EaseInOutCubic:	factor = EaseInOutCubic(factor);	break;
	case Func_EaseInQuart:		factor = EaseInQuart(factor);		break;
	case Func_EaseOutQuart:		factor = EaseOutQuart(factor);		break;
	case Func_EaseInOutQuart:	factor = EaseInOutQuart(factor);	break;
	case Func_EaseInQuint:		factor = EaseInQuint(factor);		break;
	case Func_EaseOutQuint:		factor = EaseOutQuint(factor);		break;
	case Func_EaseInOutQuint:	factor = EaseInOutQuint(factor);	break;
	case Func_EaseInExpo:		factor = EaseInExpo(factor);		break;
	case Func_EaseOutExpo:		factor = EaseOutExpo(factor);		break;
	case Func_EaseInOutExpo:	factor = EaseInOutExpo(factor);		break;
	case Func_EaseInCirc:		factor = EaseInCirc(factor);		break;
	case Func_EaseOutCirc:		factor = EaseOutCirc(factor);		break;
	case Func_EaseInOutCirc:	factor = EaseInOutCirc(factor);		break;
	case Func_EaseInBack:		factor = EaseInBack(factor);		break;
	case Func_EaseOutBack:		factor = EaseOutBack(factor);		break;
	case Func_EaseInOutBack:	factor = EaseInOutBack(factor);		break;
	case Func_EaseInElastic:	factor = EaseInElastic(factor);		break;
	case Func_EaseOutElastic:	factor = EaseOutElastic(factor);	break;
	case Func_EaseInOutElastic:	factor = EaseInOutElastic(factor);	break;
	case Func_EaseInBounce:		factor = EaseInBounce(factor);		break;
	case Func_EaseOutBounce:	factor = EaseOutBounce(factor);		break;
	case Func_EaseInOutBounce:	factor = EaseInOutBounce(factor);	break;
	default:														break;
	}


	// Interpolate the transform.
	dx::XMFLOAT3A position = dx::XMFLOAT3A(
		startPos.x + (endPos.x - startPos.x) * factor,
		startPos.y + (endPos.y - startPos.y) * factor,
		startPos.z + (endPos.z - startPos.z) * factor
	);

	// Interpolate the rotation using slerp.
	dx::XMFLOAT4A rotation;
	dx::XMStoreFloat4A(&rotation, dx::XMQuaternionSlerp(startQuat, endQuat, factor));

	// Interpolate the scale.
	dx::XMFLOAT3A scale = dx::XMFLOAT3A(
		startScale.x + (endScale.x - startScale.x) * factor,
		startScale.y + (endScale.y - startScale.y) * factor,
		startScale.z + (endScale.z - startScale.z) * factor
	);

	// Set the transform to the new values.
	_transform->SetPosition(position, ReferenceSpace::World);
	_transform->SetRotation(rotation, ReferenceSpace::World);
	_transform->SetScale(scale, ReferenceSpace::World);
}

void TimelineSequence::AddWaypoint(TimelineWaypoint waypoint)
{
	_waypoints.emplace_back(waypoint);
}

void TimelineSequence::SetType(SequenceType type)
{
	_type = type;
}

void TimelineSequence::SetType(int type)
{
	_type = static_cast<SequenceType>(type);
}

bool TimelineSequence::RemoveLastWaypoint()
{
	if (_waypoints.size() > 0)
	{
		_waypoints.pop_back();
		return true;
	}

	Warn("No waypoints to remove.");
	return true;
}

SequenceStatus TimelineSequence::Run(Transform* transform, bool loop, bool lerpToStart)
{
	// Final validation before running the sequence.
	if (!Validate())
	{
		_status = SequenceStatus::SEQUENCE_ERROR;
		return _status;
	}

	// Set up the parameters for this run.
	_loop = _type == SequenceType::SEQUENCE ? loop : false; // Loop is not valid for lerp to point sequences.
	_lerpToStart = _type == SequenceType::SEQUENCE ? lerpToStart : false; // Lerp to start is not valid for lerp to point sequences.
	_transform = transform;
	_elapsedTime = 0.0f;

	// If lerp to start is enabled, set the start position, rotation and scale.
	if (_lerpToStart)
	{
		_startPosition = transform->GetPosition(ReferenceSpace::World);
		_startRotation = transform->GetRotation(ReferenceSpace::World);
		_startScale = transform->GetScale(ReferenceSpace::World);

		_status = SequenceStatus::LERP_TO_START;
	}
	// If the sequence is a lerp to point sequence, set the start position, rotation and scale, but continue as a normal run.
	else if (_type == SequenceType::LERP_TO_POINT)
	{
		_startPosition = transform->GetPosition(ReferenceSpace::World);
		_startRotation = transform->GetRotation(ReferenceSpace::World);
		_startScale = transform->GetScale(ReferenceSpace::World);

		_status = SequenceStatus::RUNNING;
	}
	// If the sequence is a normal sequence, run it.
	else
	{
		_status = SequenceStatus::RUNNING;
	}

	return _status;
}

SequenceStatus TimelineSequence::Update(TimeUtils &time)
{
	// If the sequence is not running, return the status.
	if (_status != SequenceStatus::RUNNING && _status != SequenceStatus::LERP_TO_START)
	{
		return _status;
	}

	// TODO: Don't perform the start lerp if already at the start position.
	// Lerp to start logic.
	if (_status == SequenceStatus::LERP_TO_START)
	{
		if (_elapsedTime >= _lerpToStartTime)
		{
			_elapsedTime = 0.0f;
			_status = SequenceStatus::RUNNING;
			return _status;
		}
		else
		{
			Lerp(_transform, _elapsedTime, nullptr, &_waypoints[0]);
			_elapsedTime += time.GetDeltaTime();
			return _status;
		}
	}

	// Lerp to point logic.
	if (_type == SequenceType::LERP_TO_POINT)
	{
		// If the elapsed time is greater than the waypoint time, set the transform to the waypoint and finish the sequence.
		if (_elapsedTime >= _waypoints[0].GetTime())
		{
			_elapsedTime = 0.0f;

			_transform->SetPosition(_waypoints[0].GetPosition(), ReferenceSpace::World);
			_transform->SetRotation(_waypoints[0].GetRotation(), ReferenceSpace::World);
			_transform->SetScale(_waypoints[0].GetScale(), ReferenceSpace::World);

			_status = SequenceStatus::FINISHED;
			return _status;
		}
		// Otherwise, lerp to the waypoint.
		else
		{
			Lerp(_transform, _elapsedTime, nullptr, &_waypoints[0]);
			_elapsedTime += time.GetDeltaTime();
			return _status;
		}
	}

	// Normal sequence logic.
	TimelineWaypoint* start = nullptr;
	TimelineWaypoint* end = nullptr;

	/* Find the two waypoints that the current time is between.				 /
	/  nullptr start means the elapsed time is before the first waypoint.	 /
	/  nullptr end means the elapsed time is after the last waypoint.		*/
	for (size_t i = 0; i < _waypoints.size(); i++)
	{
		if (_waypoints[i].GetTime() <= _elapsedTime)
		{
			start = &_waypoints[i];
		}

		if (_waypoints[i].GetTime() > _elapsedTime)
		{
			end = &_waypoints[i];
			break;
		}
	}

	// If the current time is before the first waypoint, set the transform to the first waypoint.
	if (start == nullptr)
	{
		_transform->SetPosition(_waypoints[0].GetPosition(), ReferenceSpace::World);
		_transform->SetRotation(_waypoints[0].GetRotation(), ReferenceSpace::World);
		_transform->SetScale(_waypoints[0].GetScale(), ReferenceSpace::World);
	}
	// If the current time is after the last waypoint, set the transform to the last
	else if (end == nullptr)
	{
		_transform->SetPosition(_waypoints[_waypoints.size() - 1].GetPosition(), ReferenceSpace::World);
		_transform->SetRotation(_waypoints[_waypoints.size() - 1].GetRotation(), ReferenceSpace::World);
		_transform->SetScale(_waypoints[_waypoints.size() - 1].GetScale(), ReferenceSpace::World);
		
		// If the sequence is looping, reset the elapsed time.
		if (_loop)
		{
			_elapsedTime = 0.0f;

			// If lerp to start is enabled, set the start position, rotation and scale.
			if (_lerpToStart)
			{
				_startPosition = _transform->GetPosition(ReferenceSpace::World);
				_startRotation = _transform->GetRotation(ReferenceSpace::World);
				_startScale = _transform->GetScale(ReferenceSpace::World);

				_status = SequenceStatus::LERP_TO_START;
			}
			return _status;
		}
		else
		{
			_status = SequenceStatus::FINISHED;
			return _status;
		}
	}
	// If the current time is between two waypoints, interpolate between them.
	else
	{
		Lerp(_transform, _elapsedTime, start, end);
		_elapsedTime += time.GetDeltaTime();
		
		return _status;
	}

	return SequenceStatus::SEQUENCE_ERROR; // Should never reach this point. TODO: Verify with Linus.
}

SequenceStatus TimelineSequence::Stop()
{
	_status = SequenceStatus::STOPPED;
	return _status;
}

bool TimelineSequence::Validate()
{
	// Invalid if empty.
	if (_waypoints.size() == 0)
	{
		_latestError = "No waypoints in sequence.";
		return false;
	}


	if (_type == SequenceType::LERP_TO_POINT)
	{
		// Invalid if more than one waypoint.
		if (_waypoints.size() != 1)
		{
			_latestError = "Lerp to point sequence can only have one waypoint.";
			return false;
		}

		// Invalid if lerp to point waypoint is at time = 0.
		if (_waypoints[0].GetTime() == 0)
		{
			_latestError = "Lerp to point waypoint cannot be at time = 0.";
			return false;
		}

		return true;
	}
	else if (_type == SequenceType::SEQUENCE)
	{
		// Invalid if first waypoint is not at time 0.
		if (_waypoints[0].GetTime() != 0)
		{
			_latestError = "First waypoint is not at time = 0.";
			return false;
		}


		float time = _waypoints[0].GetTime();
		// Invalid if any waypoint is at the same time as another or before the previous.
		for (size_t i = 1; i < _waypoints.size(); i++)
		{
			if (_waypoints[i].GetTime() <= time)
			{
				_latestError = "Waypoint at index " + std::to_string(i) + " (time " + std::to_string(_waypoints[i].GetTime()) + ") is before the previous waypoint, which was at " + std::to_string(time) + ".";
				return false;
			}
			time = _waypoints[i].GetTime();
		}
		return true;
	}
	else
	{
		_latestError = "Invalid sequence type.";
		return false;
	}
}

bool TimelineSequence::Serialize(std::string *code) const
{
	// Serialize each waypoint.
	for (auto& waypoint : _waypoints)
	{
		// Lead the waypoint with a '<' and end it with a '>'.
		*code += "<";
		if (!waypoint.Serialize(code))
			Warn("Failed to serialize waypoint.");

		*code += ">";
	}
	return true;
}

bool TimelineSequence::Deserialize(const std::string &code)
{
	std::vector<std::string> waypoints;
	size_t start = 0, end = 0;

	// Get each waypoint from the code.
	while ((start = code.find('<', end)) != std::string::npos &&
		(end = code.find('>', start)) != std::string::npos) {
		waypoints.emplace_back(code.substr(start + 1, end - start - 1));
	}

	// Deserialize each waypoint and add it to the sequence.
	for (size_t i = 0; i < waypoints.size(); i++)
	{
		TimelineWaypoint waypoint;
		if (!waypoint.Deserialize(waypoints[i]))
			Warn("Failed to deserialize waypoint " + std::to_string(i));

		AddWaypoint(waypoint);
	}

	return true;
}

std::vector<TimelineWaypoint> &TimelineSequence::GetWaypoints()
{
	return _waypoints;
}

const TimelineWaypoint *TimelineSequence::GetWaypoint(int index)
{
	if (index < 0 || index >= _waypoints.size())
	{
		Warn("Invalid waypoint index: " + std::to_string(index));
		return nullptr;
	}
	return &_waypoints[index];
}

const std::string &TimelineSequence::GetError() const
{
	return _latestError;
}

const SequenceStatus &TimelineSequence::GetStatus() const
{
	return _status;
}

const SequenceType &TimelineSequence::GetType() const
{
	return _type;
}

float TimelineSequence::GetLength() const
{
	float length = _waypoints[_waypoints.size() - 1].GetTime();
	//for (int i = 0; i < _waypoints.size(); i++)
	//{
	//	length += _waypoints[i].GetTime();
	//}
	return length;
}

void TimelineSequence::SetEasing(EasingFunction type)
{
	_easing = type;
}
