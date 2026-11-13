#pragma once

// Shared helpers used by both parts of the lab:
//   - PARP_LR3_CPP.cpp (tasks 1-5, 9)
//   - TasksPartTwo.cpp (tasks 6-8)

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Number of timing samples; the minimum of them is reported
const int REPEAT_COUNT = 10;

// Alignment for all SIMD arrays (enough for SSE - 16, AVX - 32, cache line - 64)
const size_t SIMD_ALIGNMENT = 64;

// Processor / SIMD support -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
struct SIMDInfo {
    bool mmx, sse, sse2, sse3, ssse3, sse41, sse42;
    bool sse4a, amd_3dnow, amd_3dnow2, amd_sse5;
    bool osxsave, os_avx;
    bool avx, fma, avx2;
    int max_function;
    unsigned int ext_max_function;
    unsigned __int64 xcr0;
};

int MaxFunctionNumber(unsigned int* ExtMaxFunctionNumber);
SIMDInfo GetSIMDSupport();

// Time -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
double GetTimeSeconds();

// Calls Function once (warm-up), then REPEAT_COUNT times measures RepeatCount calls.
// Returns the minimum time of one call, in seconds.
template <typename Function>
double MeasureSeconds(Function&& Func, size_t RepeatCount) {
    Func();

    double MinTime = 1.0e9;
    for (int s = 0; s < REPEAT_COUNT; s++) {
        double Start = GetTimeSeconds();
        for (size_t r = 0; r < RepeatCount; r++) {
            Func();
        }
        double Time = (GetTimeSeconds() - Start) / RepeatCount;

        if (Time < MinTime) {
            MinTime = Time;
        }
    }
    return MinTime;
}

// Number of calls so that one sample processes about 32M bytes-elements
size_t RepeatCountForSize(size_t n);

// Memory -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void* AllocateAligned(size_t Bytes);
void FreeAligned(void* p);

// Random -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
uint32_t Random32();
float RandomFloat(float Min, float Max);

// Output -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const char* YesNo(bool Value);
std::string FormatNumber(double Value, int Precision);

// One row of a "Scalar / SSE / AVX" comparison table. Time < 0 means "not measured".
struct Result {
    std::string Type;
    size_t Size;
    double ScalarTime, SSETime, AVXTime;
};

// Prints all results with Size == n
void PrintResults(const std::vector<Result>& Results, size_t n);
