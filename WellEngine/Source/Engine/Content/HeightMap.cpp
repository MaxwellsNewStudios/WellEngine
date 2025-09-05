#include "stdafx.h"
#include "Content/HeightMap.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


void HeightMap::Initialize(const std::vector<float> &heightValues, unsigned int width, unsigned int height, std::string name)
{
	_heightValues = heightValues;
	_width = width;
	_height = height;
	_map = std::move(name);
}

std::vector<float> HeightMap::GetHeightValues() const
{
	return _heightValues;
}

unsigned int HeightMap::GetWidth() const
{
	return _width;
}

unsigned int HeightMap::GetHeight() const
{
	return _height;
}

std::string HeightMap::GetMap() const
{
	return _map;
}
