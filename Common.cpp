#include "Common.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <malloc.h>
#include <intrin.h>
#include <immintrin.h>

using namespace std;



// "Параллелизм150903_u.pdf", p. 132
int MaxFunctionNumber(unsigned int* ExtMaxFunctionNumber) {
    int a[4];
    int b = (int)0x80000000;

    if (ExtMaxFunctionNumber != NULL) {
        __cpuid(a, b);
        *ExtMaxFunctionNumber = (unsigned int)a[0];
    }

    b = 0;
    __cpuid(a, b);
    return a[0];
}

// "Пз-3.pdf", p. 43, "Параллелизм150903_u.pdf", p. 135-136
SIMDInfo GetSIMDSupport() {
    SIMDInfo Info;
    memset(&Info, 0, sizeof(Info));

    int Regs[4];
    Info.max_function = MaxFunctionNumber(&Info.ext_max_function);

    if (Info.max_function >= 1) {
        __cpuid(Regs, 1);
        int ecx = Regs[2], edx = Regs[3];

        Info.mmx = (edx >> 23) & 1;
        Info.sse = (edx >> 25) & 1;
        Info.sse2 = (edx >> 26) & 1;

        Info.sse3 = (ecx >> 0) & 1;
        Info.ssse3 = (ecx >> 9) & 1;
        Info.sse41 = (ecx >> 19) & 1;
        Info.sse42 = (ecx >> 20) & 1;
        Info.fma = (ecx >> 12) & 1;

        bool AVXBit = (ecx >> 28) & 1;

        Info.osxsave = (ecx >> 27) & 1;

        if (Info.osxsave && AVXBit) {
            Info.xcr0 = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
            Info.os_avx = (Info.xcr0 & 6) == 6;  // XMM and YMM state saved by OS
        }
        Info.avx = AVXBit && Info.os_avx;
    }

    if (Info.max_function >= 7) {
        __cpuidex(Regs, 7, 0);

        bool AVX2Bit = (Regs[1] >> 5) & 1;

        Info.avx2 = AVX2Bit && Info.fma && Info.avx;
    }

    if (Info.ext_max_function >= 0x80000001u) {
        __cpuid(Regs, (int)0x80000001);

        Info.sse4a = (Regs[2] >> 6) & 1;
        Info.amd_sse5 = (Regs[2] >> 11) & 1;
        Info.amd_3dnow = (Regs[3] >> 31) & 1;
        Info.amd_3dnow2 = (Regs[3] >> 30) & 1;
    }

    return Info;
}



// https://en.cppreference.com/w/cpp/chrono/steady_clock
double GetTimeSeconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

size_t RepeatCountForSize(size_t n) {
    size_t RepeatCount = (size_t)((1ull << 25) / n);
    if (RepeatCount < 2) {
        RepeatCount = 2;
    }
    if (RepeatCount > 200000) {
        RepeatCount = 200000;
    }
    return RepeatCount;
}



// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc
void* AllocateAligned(size_t Bytes) {
    return _aligned_malloc(Bytes, SIMD_ALIGNMENT);
}

void FreeAligned(void* p) {
    _aligned_free(p);
}



uint32_t Random32() {
    return ((uint32_t)rand() << 16) ^ ((uint32_t)rand() << 1) ^ (uint32_t)rand();
}

float RandomFloat(float Min, float Max) {
    float Fraction = (float)rand() / RAND_MAX;
    return Min + (Max - Min) * Fraction;
}



const char* YesNo(bool Value) {
    return Value ? "yes" : "no";
}

string FormatNumber(double Value, int Precision) {
    char Text[64];
    snprintf(Text, sizeof(Text), "%.*f", Precision, Value);
    return Text;
}

void PrintResults(const vector<Result>& Results, size_t n) {
    cout << "Array size = " << n << "\n";
    cout << left << setw(8) << "Type" << right
        << setw(18) << "Scalar, microsec" << setw(18) << "SSE, microsec" << setw(18) << "AVX, microsec"
        << setw(14) << "SSE speedup" << setw(14) << "AVX speedup" << "\n";

    for (size_t i = 0; i < Results.size(); i++) {
        const Result& r = Results[i];
        if (r.Size != n) {
            continue;
        }

        string SSETimeText = "-";
        string AVXTimeText = "-";
        string SSESpeedupText = "-";
        string AVXSpeedupText = "-";

        if (r.SSETime >= 0) {
            SSETimeText = FormatNumber(r.SSETime * 1e6, 3);
            SSESpeedupText = FormatNumber(r.ScalarTime / r.SSETime, 2);
        }
        if (r.AVXTime >= 0) {
            AVXTimeText = FormatNumber(r.AVXTime * 1e6, 3);
            AVXSpeedupText = FormatNumber(r.ScalarTime / r.AVXTime, 2);
        }

        cout << left << setw(8) << r.Type << right
            << setw(18) << FormatNumber(r.ScalarTime * 1e6, 3) << setw(18) << SSETimeText << setw(18) << AVXTimeText
            << setw(14) << SSESpeedupText << setw(14) << AVXSpeedupText << "\n";
    }
    cout << "\n";
}
