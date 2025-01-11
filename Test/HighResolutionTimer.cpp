//
// Copyright 2024 Axel Walthelm
//

#include "HighResolutionTimer.h"

#ifdef _WIN32
#include <windows.h>
#include <stdexcept>


HighResolutionTime_t GetHighResolutionTime()
{
	LARGE_INTEGER counts;
	QueryPerformanceCounter(&counts);
	return uint64_t(counts.QuadPart);
}

uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start, HighResolutionTime_t stop)
{
	if (stop < start)
		throw std::overflow_error("negative time");

	uint64_t ticks = stop - start;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	uint64_t f = uint64_t(frequency.QuadPart);
	constexpr uint64_t n = 1000000000;
	if (ticks < UINT64_MAX / n)
		return ticks * n / f;

	if (f >= UINT64_MAX / n)
		throw std::overflow_error("timer resolution too high"); // consider to use 128 bit division
	uint64_t seconds = ticks / f;
	uint64_t nanoseconds = (ticks - seconds * f) * n / f;
	return seconds * n + nanoseconds;
}

uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start)
{
	return GetHighResolutionTimeElapsedNs(start, GetHighResolutionTime());
}

#else

#include <iostream>
#include <time.h>
#include <stdexcept>

HighResolutionTime_t GetHighResolutionTime()
{
	constexpr uint64_t n = 1000000000;
	timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);
	// The combined value may overflow, but when using unsigned arithmetic the C++ standard asserts that
	// it will wrap around at maximum value, so it still can be used for valid 64 bit time differences.
	return uint64_t((uint64_t)t.tv_sec * n + (uint64_t)t.tv_nsec);
}

uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start, HighResolutionTime_t stop)
{
	static_assert(std::is_same<HighResolutionTime_t, uint64_t>::value, "HighResolutionTime_t is not uint64_t");

	uint64_t diff = stop - start;
	if ((int64_t)diff < 0)
		throw std::overflow_error("negative time");

	return diff;
}

uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start)
{
	return GetHighResolutionTimeElapsedNs(start, GetHighResolutionTime());
}

#endif
