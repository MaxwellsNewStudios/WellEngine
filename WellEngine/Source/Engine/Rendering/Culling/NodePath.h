#pragma once
#include <cstdint>

struct TreePath
{
	// TODO: Implement

	// Each step in the tree can be represented by 2 bits, denoting the index of the 
	// node that was stepped to. However, we must also be able to specify where the 
	// path ends. Due to this, every depth is represented by 3 bits, with the first 
	// bit indicating if the end of path has been reached. The leftover bit is unused.
	// 
	// Ex: (We will format the flag in pairs of 3, instead of the more common 4-pair)
	//   [0 000 000 000 000 000] -> root[0][0][0][0][0]
	//   [0 000 000 001 010 110] -> root[3][1]
	//   [0 011 101 010 000 100] -> root[2][0][1]

	uint16_t bitPath = 0b001u; // Default is end at root
};
