#pragma once

#include <vector>

class HeightMap 
{
private:
	std::vector<float> _heightValues;
	unsigned int _width = 0, _height = 0;
	std::string _map = "";

public:
	HeightMap() = default;
	~HeightMap() = default;

	void Initialize(const std::vector<float> &heightValues, unsigned int width, unsigned int height, std::string name);

	std::vector<float> GetHeightValues() const;
	unsigned int GetWidth() const;
	unsigned int GetHeight() const;
	std::string GetMap() const;

	TESTABLE()
};
