#include "HighResolutionTimer.h"
#include <windows.h>
#include <immintrin.h>
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
		throw new std::overflow_error("negative time");

	uint64_t ticks = stop - start;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	uint64_t f = uint64_t(frequency.QuadPart);
	constexpr uint64_t n = 1000000000;
	if (ticks < UINT64_MAX / n)
		return ticks * n / f;

	if (f >= UINT64_MAX / n)
		throw new std::overflow_error("timer resolution too high"); // consider to use 128 bit division
	uint64_t seconds = ticks / f;
	uint64_t nanoseconds = (ticks - seconds * f) * n / f;
	return seconds * n + nanoseconds;
}

uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start)
{
	return GetHighResolutionTimeElapsedNs(start, GetHighResolutionTime());
}
