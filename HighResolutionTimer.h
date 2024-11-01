#pragma once
#include <stdint.h>

typedef uint64_t HighResolutionTime_t;

HighResolutionTime_t GetHighResolutionTime();

uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start, HighResolutionTime_t stop);
uint64_t GetHighResolutionTimeElapsedNs(HighResolutionTime_t start);
