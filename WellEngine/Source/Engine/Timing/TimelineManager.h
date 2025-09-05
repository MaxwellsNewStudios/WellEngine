#pragma once
#include <unordered_map>

#include "Timing/TimelineSequence.h"

class TimelineManager
{
private:
	std::unordered_map<std::string, TimelineSequence> _sequences;
	std::vector<std::string> _runningSequences;

#ifdef USE_IMGUI
	bool creatingSequence = false;
	int selectedSequenceIndex = -1;
	std::string selectedSequence = "";
	TimelineSequence newSequence = TimelineSequence();
#endif

public:
	SequenceStatus RunSequence(const std::string &sequenceName, Transform* transform, bool loop = false, bool lerpToStart = true);
	SequenceStatus StopSequence(const std::string &sequenceName);

	bool Update(TimeUtils &time);
	bool AddSequence(const std::string &sequenceName, TimelineSequence sequence);
	bool RemoveSequence(const std::string &sequenceName);

	bool Serialize(std::string *code);
	bool Deserialize();
	
#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI(Transform* transform);
#endif

	[[nodiscard]] std::unordered_map<std::string, TimelineSequence> GetSequences() const;
	[[nodiscard]] TimelineSequence GetSequence(const std::string &sequenceName) const;

	TESTABLE()
};