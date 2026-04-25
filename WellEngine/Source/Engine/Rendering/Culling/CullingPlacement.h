#pragma once

#include <vector>

namespace WellEngine::Culling
{
	struct TreePath
	{
		std::vector<uint8_t> steps;
	};

	struct CullingPlacement
	{
		TreePath quadTreePath;
	};
};
