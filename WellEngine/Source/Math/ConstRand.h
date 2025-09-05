// Functions for generating random numbers at compile time.

#pragma once
#include <string>

#define RandomUniqueColor() (const uint32_t)ConstRand(sizeof(__FILE__) * (const size_t)__LINE__, (const size_t)0x000000, (const size_t)0xFFFFFF)

// This is a simple implementation of a linear congruential generator (LCG).
constexpr size_t ConstRand(const size_t seed, const size_t min, const size_t max)
{
	// Constants for the LCG
	constexpr size_t a = 1664525;
	constexpr size_t c = 1013904223;
	constexpr size_t m = 4294967296;

	// Generate the next random number
	const size_t rnd = (a * seed + c) % m;

	// Scale the result to the desired range
	return min + (rnd % (max - min));
}