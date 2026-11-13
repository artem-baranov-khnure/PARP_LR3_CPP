#define NOMINMAX

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <windows.h>
#include <intrin.h>
#include "TasksPartTwo.h"

using namespace std;


const int REPEAT_COUNT = 10;



// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// https://en.cppreference.com/w/cpp/chrono/steady_clock
double GetTimeSeconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc
void* AllocateAligned(size_t Bytes) {
    return _aligned_malloc(Bytes, 64);
}

void FreeAligned(void* p) {
    _aligned_free(p);
}

const char* YesNo(bool Value) {
    return Value ? "yes" : "no";
}

uint32_t Random32() {
    return ((uint32_t)rand() << 16) ^ ((uint32_t)rand() << 1) ^ (uint32_t)rand();
}



// Task 1 -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// "Пз-3.pdf", p. 41-43
typedef enum {
    INTEL, AMD, OTHER
} ProcessorType;

const char* GetProcessorTypeName(ProcessorType Type) {
    if (Type == INTEL) {
        return "INTEL";
    }
    if (Type == AMD) {
        return "AMD";
    }

    return "OTHER";
}

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

// "Пз-3.pdf", p. 41-42
ProcessorType DefineProcessorType(char ProcessorName[13]) {
    ProcessorType Type = OTHER;
    char INTELName[13] = "GenuineIntel";
    char AMDName[13] = "AuthenticAMD";
    int Regs[4];

    ProcessorName[12] = 0;
    __cpuid(Regs, 0);
    memcpy(ProcessorName, &Regs[1], 4);
    memcpy(ProcessorName + 4, &Regs[3], 4);
    memcpy(ProcessorName + 8, &Regs[2], 4);

    if (memcmp(INTELName, ProcessorName, 12) == 0)
        Type = INTEL;
    else {
        if (memcmp(AMDName, ProcessorName, 12) == 0)
            Type = AMD;
    }
    return Type;
}

// "Пз-3.pdf", p. 42
bool DefineExtendProcessorType(char BrandString[49]) {
    unsigned int ExtMaxFunctionNumber = 0;
    MaxFunctionNumber(&ExtMaxFunctionNumber);

    BrandString[0] = 0;
    if (ExtMaxFunctionNumber < 0x80000004u) {
        return false;
    }

    int Regs[4];
    char* pBrandString = BrandString;
    for (unsigned uCommand = 0x80000002; uCommand < 0x80000005; uCommand++) {
        __cpuid(Regs, (int)uCommand);
        memcpy(pBrandString, &Regs[0], 4); pBrandString += 4;
        memcpy(pBrandString, &Regs[1], 4); pBrandString += 4;
        memcpy(pBrandString, &Regs[2], 4); pBrandString += 4;
        memcpy(pBrandString, &Regs[3], 4); pBrandString += 4;
    }
    *pBrandString = 0;

    return true;
}

struct SIMDInfo {
    bool mmx, sse, sse2, sse3, ssse3, sse41, sse42;
    bool sse4a, amd_3dnow, amd_3dnow2, amd_sse5;
    bool osxsave, os_avx;
    bool avx, fma, avx2;
    int max_function;
    unsigned int ext_max_function;
    unsigned __int64 xcr0;
};

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
            Info.os_avx = (Info.xcr0 & 4) != 0;
        }
        Info.avx = AVXBit && Info.os_avx;
    }

    if (Info.max_function >= 7) {
        __cpuid(Regs, 7);

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

// "Пз-3.pdf", p. 40-41, 43
bool AVX2ProcessorSupport() {
    bool b = false; int reg[4];
    __cpuidex(reg, 0, 0);
    if (reg[0] >= 7) {
        __cpuidex(reg, 1, 0); b = ((reg[2] >> 12) & 1) != 0;
        if (b) {
            __cpuidex(reg, 7, 0); b = ((reg[1] >> 5) & 1) != 0;
        }
    }
    return b;
}

bool AVX2OSSupport() {
    int reg[4];

    __cpuid(reg, 1);
    if (((reg[2] >> 27) & 1) == 0) {
        return false;
    }

    return (_xgetbv(_XCR_XFEATURE_ENABLED_MASK) & 4) != 0;
}

bool AVX2Support() {
    bool b = AVX2ProcessorSupport();
    if (b) b = AVX2OSSupport();
    return b;
}

const char* GetWidestIntegerName(const SIMDInfo& Info) {
    if (Info.avx2) {
        return "AVX2 (256 bit)";
    }
    if (Info.sse42) {
        return "SSE4.2 (128 bit)";
    }
    if (Info.sse2) {
        return "SSE2 (128 bit)";
    }
    return "none";
}

const char* GetWidestFloatName(const SIMDInfo& Info) {
    if (Info.avx) {
        return "AVX (256 bit)";
    }
    if (Info.sse) {
        return "SSE (128 bit)";
    }
    return "none";
}

void PrintProcessorInfo(const SIMDInfo& Info, ProcessorType RecommendedType) {
    char ProcessorName[13], BrandString[49];
    ProcessorType Type = DefineProcessorType(ProcessorName);
    bool HasBrand = DefineExtendProcessorType(BrandString);

    const char* BrandName = BrandString;
    while (*BrandName == ' ') {
        BrandName++;
    }

    cout << left;
    cout << setw(28) << "Vendor (CPUID.0)" << ": " << ProcessorName << "\n";
    cout << setw(28) << "Processor type" << ": " << GetProcessorTypeName(Type) << "\n";

    if (HasBrand) {
        cout << setw(28) << "Processor name" << ": " << BrandName << "\n";
    }

    cout << setw(28) << "Max function (basic)" << ": " << Info.max_function << "\n";
    cout << setw(28) << "Max function (extended)" << ": 0x" << hex << Info.ext_max_function << dec << "\n";
    cout << "\n";

    cout << setw(28) << "MMX" << ": " << YesNo(Info.mmx) << "\n";
    cout << setw(28) << "SSE" << ": " << YesNo(Info.sse) << "\n";
    cout << setw(28) << "SSE2" << ": " << YesNo(Info.sse2) << "\n";
    cout << setw(28) << "SSE3" << ": " << YesNo(Info.sse3) << "\n";
    cout << setw(28) << "SSSE3" << ": " << YesNo(Info.ssse3) << "\n";
    cout << setw(28) << "SSE4.1" << ": " << YesNo(Info.sse41) << "\n";
    cout << setw(28) << "SSE4.2" << ": " << YesNo(Info.sse42) << "\n";
    cout << setw(28) << "FMA" << ": " << YesNo(Info.fma) << "\n";
    cout << setw(28) << "AVX" << ": " << YesNo(Info.avx) << "\n";
    cout << setw(28) << "AVX2" << ": " << YesNo(Info.avx2) << "\n";

    if (Type == AMD) {
        cout << setw(28) << "SSE4A" << ": " << YesNo(Info.sse4a) << "\n";
        cout << setw(28) << "3DNow!" << ": " << YesNo(Info.amd_3dnow) << "\n";
        cout << setw(28) << "3DNow!2" << ": " << YesNo(Info.amd_3dnow2) << "\n";
        cout << setw(28) << "SSE5" << ": " << YesNo(Info.amd_sse5) << "\n";
    }
    cout << "\n";

    cout << setw(28) << "OS saves YMM registers" << ": " << YesNo(Info.os_avx) << "\n";
    cout << setw(28) << "AVX2Support()" << ": " << (AVX2Support() ? "true" : "false") << "\n";
    cout << setw(28) << "Widest block, integer" << ": " << GetWidestIntegerName(Info) << "\n";
    cout << setw(28) << "Widest block, float/double" << ": " << GetWidestFloatName(Info) << "\n";
    cout << "\n";

    cout << setw(28) << "Recommended type" << ": " << GetProcessorTypeName(RecommendedType) << "\n";
    cout << setw(28) << "Detected type" << ": " << GetProcessorTypeName(Type) << "\n";
    cout << setw(28) << "Match" << ": " << YesNo(RecommendedType == Type) << "\n";
}



// Task 2 -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// "Пз-3.pdf", p. 44, example 4
template <typename T>
void AbsSum(const T* y, const T* z, T* x, size_t n) {
#pragma loop(no_vector)
    for (size_t i = 0; i < n; i++) {
        T a = y[i] < 0 ? (T)(-y[i]) : y[i];
        T b = z[i] < 0 ? (T)(-z[i]) : z[i];

        x[i] = (T)(a + b);
    }
}

template <typename T>
void AbsSumTail(const T* y, const T* z, T* x, size_t Start, size_t n) {
    for (size_t i = Start; i < n; i++) {
        T a = y[i] < 0 ? (T)(-y[i]) : y[i];
        T b = z[i] < 0 ? (T)(-z[i]) : z[i];

        x[i] = (T)(a + b);
    }
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
inline __m128i SSEAbs64(__m128i a) {
    __m128i sign = _mm_cmpgt_epi64(_mm_setzero_si128(), a);
    return _mm_sub_epi64(_mm_xor_si128(a, sign), sign);
}

// "Пз-3.pdf", p. 44
inline __m128 SSEAbsF(__m128 a) {
    __m128 zero = _mm_setzero_ps(), r1, r2, r3;
    r1 = _mm_cmple_ps(zero, a);
    r2 = _mm_and_ps(r1, a);
    r3 = _mm_andnot_ps(r1, a);
    r3 = _mm_sub_ps(zero, r3);
    return _mm_add_ps(r2, r3);
}

inline __m128d SSEAbsD(__m128d a) {
    __m128d zero = _mm_setzero_pd(), r1, r2, r3;
    r1 = _mm_cmple_pd(zero, a);
    r2 = _mm_and_pd(r1, a);
    r3 = _mm_andnot_pd(r1, a);
    r3 = _mm_sub_pd(zero, r3);
    return _mm_add_pd(r2, r3);
}

// "Параллелизм150903_u.pdf", p. 109-110
void SSEAbsSum8(const int8_t* y, const int8_t* z, int8_t* x, size_t n) {
    size_t i;
    __m128i* py = (__m128i*)y;
    __m128i* pz = (__m128i*)z;
    __m128i* px = (__m128i*)x;
    for (i = 0; i < n / 16; ++i)
        px[i] = _mm_add_epi8(_mm_abs_epi8(py[i]), _mm_abs_epi8(pz[i]));
    AbsSumTail(y, z, x, i * 16, n);
}

void SSEAbsSum16(const int16_t* y, const int16_t* z, int16_t* x, size_t n) {
    size_t i;
    __m128i* py = (__m128i*)y;
    __m128i* pz = (__m128i*)z;
    __m128i* px = (__m128i*)x;
    for (i = 0; i < n / 8; ++i)
        px[i] = _mm_add_epi16(_mm_abs_epi16(py[i]), _mm_abs_epi16(pz[i]));
    AbsSumTail(y, z, x, i * 8, n);
}

void SSEAbsSum32(const int32_t* y, const int32_t* z, int32_t* x, size_t n) {
    size_t i;
    __m128i* py = (__m128i*)y;
    __m128i* pz = (__m128i*)z;
    __m128i* px = (__m128i*)x;
    for (i = 0; i < n / 4; ++i)
        px[i] = _mm_add_epi32(_mm_abs_epi32(py[i]), _mm_abs_epi32(pz[i]));
    AbsSumTail(y, z, x, i * 4, n);
}

void SSEAbsSum64(const int64_t* y, const int64_t* z, int64_t* x, size_t n) {
    size_t i;
    __m128i* py = (__m128i*)y;
    __m128i* pz = (__m128i*)z;
    __m128i* px = (__m128i*)x;
    for (i = 0; i < n / 2; ++i)
        px[i] = _mm_add_epi64(SSEAbs64(py[i]), SSEAbs64(pz[i]));
    AbsSumTail(y, z, x, i * 2, n);
}

// "Параллелизм150903_u.pdf", p. 84-85
void SSEAbsSumF(const float* y, const float* z, float* x, size_t n) {
    size_t i;
    __m128* py = (__m128*)y;
    __m128* pz = (__m128*)z;
    __m128* px = (__m128*)x;
    for (i = 0; i < n / 4; ++i)
        px[i] = _mm_add_ps(SSEAbsF(py[i]), SSEAbsF(pz[i]));
    AbsSumTail(y, z, x, i * 4, n);
}

void SSEAbsSumD(const double* y, const double* z, double* x, size_t n) {
    size_t i;
    __m128d* py = (__m128d*)y;
    __m128d* pz = (__m128d*)z;
    __m128d* px = (__m128d*)x;
    for (i = 0; i < n / 2; ++i)
        px[i] = _mm_add_pd(SSEAbsD(py[i]), SSEAbsD(pz[i]));
    AbsSumTail(y, z, x, i * 2, n);
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// "Пз-3.pdf", p. 45
inline __m256 AVXAbsF(__m256 a) {
    __m256 zero = _mm256_setzero_ps(), r1, r2, r3;
    r1 = _mm256_cmp_ps(zero, a, _CMP_LE_OS);
    r2 = _mm256_and_ps(r1, a);
    r3 = _mm256_andnot_ps(r1, a);
    r3 = _mm256_sub_ps(zero, r3);

    return _mm256_add_ps(r2, r3);
}

inline __m256d AVXAbsD(__m256d a) {
    __m256d zero = _mm256_setzero_pd(), r1, r2, r3;
    r1 = _mm256_cmp_pd(zero, a, _CMP_LE_OS);
    r2 = _mm256_and_pd(r1, a);
    r3 = _mm256_andnot_pd(r1, a);
    r3 = _mm256_sub_pd(zero, r3);

    return _mm256_add_pd(r2, r3);
}

void AVXAbsSumF(const float* y, const float* z, float* x, size_t n) {
    size_t i;
    __m256* py = (__m256*)y;
    __m256* pz = (__m256*)z;
    __m256* px = (__m256*)x;

    for (i = 0; i < n / 8; ++i)
        px[i] = _mm256_add_ps(AVXAbsF(py[i]), AVXAbsF(pz[i]));
    AbsSumTail(y, z, x, i * 8, n);
}

void AVXAbsSumD(const double* y, const double* z, double* x, size_t n) {
    size_t i;
    __m256d* py = (__m256d*)y;
    __m256d* pz = (__m256d*)z;
    __m256d* px = (__m256d*)x;

    for (i = 0; i < n / 4; ++i)
        px[i] = _mm256_add_pd(AVXAbsD(py[i]), AVXAbsD(pz[i]));
    AbsSumTail(y, z, x, i * 4, n);
}



// Task 3, 4 -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
template <typename T>
void FillArray(T* a, size_t n) {
    for (size_t i = 0; i < n; i++) {
        a[i] = (T)(rand() % 101 - 50);
    }
}

template <typename T>
bool CheckAbsSum(const T* y, const T* z, const T* x, size_t n) {
    for (size_t i = 0; i < n; i++) {
        T a = y[i] < 0 ? (T)(-y[i]) : y[i];
        T b = z[i] < 0 ? (T)(-z[i]) : z[i];

        if (!(x[i] == (T)(a + b))) {
            return false;
        }
    }
    return true;
}

template <typename T>
using AbsSumFunction = void (*)(const T*, const T*, T*, size_t);

template <typename T>
double TimeAbsSum(AbsSumFunction<T> Function, const T* y, const T* z, T* x, size_t n) {
    size_t RepeatCount = (size_t)((1ull << 25) / n);
    if (RepeatCount < 2) {
        RepeatCount = 2;
    }

    if (RepeatCount > 200000) {
        RepeatCount = 200000;
    }

    Function(y, z, x, n);

    double MinTime = 1.0e9;
    for (int s = 0; s < REPEAT_COUNT; s++) {
        double Start = GetTimeSeconds();

        for (size_t r = 0; r < RepeatCount; r++) {
            Function(y, z, x, n);
        }
        double Time = (GetTimeSeconds() - Start) / RepeatCount;

        if (Time < MinTime) {
            MinTime = Time;
        }
    }
    return MinTime;
}

struct Result {
    string Type;
    size_t Size;
    double ScalarTime, SSETime, AVXTime;
};

template <typename T>
void RunAbsSum(const char* TypeName, size_t n, AbsSumFunction<T> ScalarFunction,
    AbsSumFunction<T> SSEFunction, const SIMDInfo& Info, vector<Result>& Results,
    AbsSumFunction<T> AVXFunction = nullptr, bool UseAVX = false) {

    size_t Bytes = n * sizeof(T);
    T* y = (T*)AllocateAligned(Bytes);
    T* z = (T*)AllocateAligned(Bytes);
    T* x = (T*)AllocateAligned(Bytes);

    if (y == NULL || z == NULL || x == NULL) {

        cout << "Not enough memory: " << TypeName << ", size = " << n << endl;

        FreeAligned(y);
        FreeAligned(z);
        FreeAligned(x);

        return;
    }

    FillArray(y, n);
    FillArray(z, n);

    Result Res;
    Res.Type = TypeName;
    Res.Size = n;
    Res.SSETime = -1;
    Res.AVXTime = -1;
    bool Correct = true;

    memset(x, 0, Bytes);
    Res.ScalarTime = TimeAbsSum<T>(ScalarFunction, y, z, x, n);
    Correct = Correct && CheckAbsSum(y, z, x, n);

    if (Info.sse42) {
        memset(x, 0, Bytes);
        Res.SSETime = TimeAbsSum<T>(SSEFunction, y, z, x, n);
        Correct = Correct && CheckAbsSum(y, z, x, n);
    }

    if (UseAVX) {
        memset(x, 0, Bytes);
        Res.AVXTime = TimeAbsSum<T>(AVXFunction, y, z, x, n);
        Correct = Correct && CheckAbsSum(y, z, x, n);
    }

    if (!Correct) {
        cout << "Wrong result: " << TypeName << ", size = " << n << endl;
    }
    Results.push_back(Res);

    FreeAligned(y);
    FreeAligned(z);
    FreeAligned(x);
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



// Task 9 -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// "Параллелизм150903_u.pdf", p. 121
void shlBit(const uint32_t* up, uint32_t* ures) {
    memset(ures, 0, 64);
    for (int k = 1; k < 512; k++) {
        uint32_t bit = (up[(k - 1) >> 5] >> ((k - 1) & 31)) & 1u;
        ures[k >> 5] |= bit << (k & 31);
    }
}

void shl(const unsigned* up, unsigned* ures) {
#pragma loop(no_vector)
    for (int i = 15; i >= 1; --i) {
        ures[i] = (up[i] << 1) | (up[i - 1] >> 31);
    }
    ures[0] = up[0] << 1;
}

void shl64(const unsigned* up, unsigned* ures) {
    unsigned __int64 a[8], r[8];
    memcpy(a, up, 64);

#pragma loop(no_vector)
    for (int i = 7; i >= 1; --i) {
        r[i] = (a[i] << 1) | (a[i - 1] >> 63);
    }
    r[0] = a[0] << 1;

    memcpy(ures, r, 64);
}

void SSEshl(const unsigned* up, unsigned* ures) {
    __m128i* p = (__m128i*)up;
    __m128i* res = (__m128i*)ures;
    __m128i a = _mm_set1_epi32((int)0x80000000);
    __m128i dop[4];

    dop[0] = _mm_cvtsi32_si128(up[3] >> 31);
    dop[1] = _mm_cvtsi32_si128(up[7] >> 31);
    dop[2] = _mm_cvtsi32_si128(up[11] >> 31);

    res[0] = _mm_or_si128(_mm_slli_epi32(p[0], 1),
        _mm_srli_epi32(_mm_slli_si128(_mm_and_si128(a, p[0]), 4), 31));
    res[1] = _mm_or_si128(_mm_or_si128(_mm_slli_epi32(p[1], 1),
        _mm_srli_epi32(_mm_slli_si128(_mm_and_si128(a, p[1]), 4), 31)),
        dop[0]);
    res[2] = _mm_or_si128(_mm_or_si128(_mm_slli_epi32(p[2], 1),
        _mm_srli_epi32(_mm_slli_si128(_mm_and_si128(a, p[2]), 4), 31)),
        dop[1]);
    res[3] = _mm_or_si128(_mm_or_si128(_mm_slli_epi32(p[3], 1),
        _mm_srli_epi32(_mm_slli_si128(_mm_and_si128(a, p[3]), 4), 31)),
        dop[2]);
}

void SSEshl64(const unsigned* up, unsigned* ures) {
    const __m128i* p = (const __m128i*)up;
    __m128i* res = (__m128i*)ures;

    __m128i v0 = _mm_load_si128(p + 0), v1 = _mm_load_si128(p + 1);
    __m128i v2 = _mm_load_si128(p + 2), v3 = _mm_load_si128(p + 3);

    __m128i c0 = _mm_srli_epi64(v0, 63), c1 = _mm_srli_epi64(v1, 63);
    __m128i c2 = _mm_srli_epi64(v2, 63), c3 = _mm_srli_epi64(v3, 63);

    _mm_store_si128(res + 0, _mm_or_si128(_mm_slli_epi64(v0, 1), _mm_slli_si128(c0, 8)));
    _mm_store_si128(res + 1, _mm_or_si128(_mm_or_si128(_mm_slli_epi64(v1, 1), _mm_slli_si128(c1, 8)),
        _mm_srli_si128(c0, 8)));
    _mm_store_si128(res + 2, _mm_or_si128(_mm_or_si128(_mm_slli_epi64(v2, 1), _mm_slli_si128(c2, 8)),
        _mm_srli_si128(c1, 8)));
    _mm_store_si128(res + 3, _mm_or_si128(_mm_or_si128(_mm_slli_epi64(v3, 1), _mm_slli_si128(c3, 8)),
        _mm_srli_si128(c2, 8)));
}

typedef void (*ShiftBatchFunction)(const uint32_t* source, uint32_t* result, size_t count);

void shlBitBatch(const uint32_t* source, uint32_t* result, size_t count) {
    for (size_t k = 0; k < count; k++) shlBit(source + 16 * k, result + 16 * k);
}

void shlBatch(const uint32_t* source, uint32_t* result, size_t count) {
    for (size_t k = 0; k < count; k++) shl(source + 16 * k, result + 16 * k);
}

void shl64Batch(const uint32_t* source, uint32_t* result, size_t count) {
    for (size_t k = 0; k < count; k++) shl64(source + 16 * k, result + 16 * k);
}

void SSEshlBatch(const uint32_t* source, uint32_t* result, size_t count) {
    for (size_t k = 0; k < count; k++) SSEshl(source + 16 * k, result + 16 * k);
}

void SSEshl64Batch(const uint32_t* source, uint32_t* result, size_t count) {
    for (size_t k = 0; k < count; k++) SSEshl64(source + 16 * k, result + 16 * k);
}

struct ShiftVariant {
    const char* Name;
    ShiftBatchFunction Function;
    bool Supported;
};


bool CheckShift(const vector<ShiftVariant>& Variants) {
    const size_t count = 1000;
    uint32_t* source = (uint32_t*)AllocateAligned(count * 64);
    uint32_t* reference = (uint32_t*)AllocateAligned(count * 64);
    uint32_t* result = (uint32_t*)AllocateAligned(count * 64);

    if (source == NULL || reference == NULL || result == NULL) {
        FreeAligned(source);
        FreeAligned(reference);
        FreeAligned(result);

        return false;
    }

    for (size_t i = 0; i < count * 16; i++) {
        source[i] = Random32();
    }

    for (size_t i = 0; i < 16; i++) {
        source[i] = 0xFFFFFFFFu;
    }

    for (size_t i = 16; i < 32; i++) {
        source[i] = (i % 4 == 3) ? 0x80000000u : 0;
    }

    for (size_t i = 32; i < 48; i++) {
        source[i] = (i % 2 == 1) ? 0x80000000u : 0;
    }

    for (size_t i = 48; i < 64; i++) {
        source[i] = 0;
    }

    source[48] = 1;

    for (size_t i = 64; i < 80; i++) {
        source[i] = 0;
    }

    source[79] = 0x80000000u;

    shlBitBatch(source, reference, count);

    bool AllCorrect = true;
    for (size_t v = 0; v < Variants.size(); v++) {
        if (!Variants[v].Supported) {
            continue;
        }

        memset(result, 0, count * 64);
        Variants[v].Function(source, result, count);

        bool Correct = memcmp(reference, result, count * 64) == 0;
        if (!Correct) {
            cout << "Wrong result: " << Variants[v].Name << "\n";
        }
        AllCorrect = AllCorrect && Correct;
    }

    FreeAligned(source);
    FreeAligned(reference);
    FreeAligned(result);

    return AllCorrect;
}

void RunShift(const SIMDInfo& Info) {
    vector<ShiftVariant> Variants;
    Variants.push_back({ "scalar uint32", shlBatch, true });
    Variants.push_back({ "scalar uint64", shl64Batch, true });
    Variants.push_back({ "SSE epi32", SSEshlBatch, Info.sse42 });
    Variants.push_back({ "SSE epi64", SSEshl64Batch, Info.sse42 });

    if (!CheckShift(Variants)) {
        cout << "Check failed" << endl;
        return;
    }

    size_t count = 1u << 10;
    uint32_t* a = (uint32_t*)AllocateAligned(count * 64);
    uint32_t* b = (uint32_t*)AllocateAligned(count * 64);

    if (a == NULL || b == NULL) {
        cout << "Not enough memory" << endl;
        FreeAligned(a);
        FreeAligned(b);
        return;
    }

    for (size_t i = 0; i < count * 16; i++) a[i] = Random32();
    memcpy(b, a, count * 64);

    cout << "Numbers (512 bit) = " << count << "\n";
    cout << left << setw(16) << "Variant" << right
        << setw(14) << "Time, microsec" << setw(14) << "ns / shift" << setw(12) << "Speedup" << "\n";

    double BaseTime = 0;
    for (size_t v = 0; v < Variants.size(); v++) {
        if (!Variants[v].Supported) {
            cout << left << setw(16) << Variants[v].Name << right
                << setw(14) << "-" << setw(14) << "-" << setw(12) << "-" << "\n";
            continue;
        }

        size_t RepeatCount = 20000;

        Variants[v].Function(a, b, count);

        double MinTime = 1.0e9;
        for (int s = 0; s < REPEAT_COUNT; s++) {
            uint32_t* source = a;
            uint32_t* result = b;

            double Start = GetTimeSeconds();
            for (size_t r = 0; r < RepeatCount; r++) {
                Variants[v].Function(source, result, count);
                uint32_t* temp = source;
                source = result;
                result = temp;
            }
            double Time = (GetTimeSeconds() - Start) / RepeatCount;

            if (Time < MinTime) {
                MinTime = Time;
            }
        }

        if (BaseTime == 0) {
            BaseTime = MinTime;
        }

        cout << left << setw(16) << Variants[v].Name << right
            << setw(14) << FormatNumber(MinTime * 1e6, 3)
            << setw(14) << FormatNumber(MinTime * 1e9 / count, 3)
            << setw(12) << FormatNumber(BaseTime / MinTime, 2) << "\n";
    }

    FreeAligned(a);
    FreeAligned(b);
}



int main()
{
    srand(12345);
    const char* separator = "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=";
    SIMDInfo Info = GetSIMDSupport();


    cout << "Task 1 " << separator << endl << endl;

    const ProcessorType RecommendedType = INTEL;
    PrintProcessorInfo(Info, RecommendedType);



    cout << "\n\n\n" << endl;
    cout << "Task 2, 3, 4, 5 " << separator << endl << endl;

    vector<size_t> Sizes;
    Sizes.push_back(1000);
    Sizes.push_back(1000 * 1000);
    Sizes.push_back(4096 * 4096);

    vector<Result> Results;
    for (size_t k = 0; k < Sizes.size(); k++) {
        size_t n = Sizes[k];

        RunAbsSum<int8_t>("int8", n, AbsSum<int8_t>, SSEAbsSum8, Info, Results);
        RunAbsSum<int16_t>("int16", n, AbsSum<int16_t>, SSEAbsSum16, Info, Results);
        RunAbsSum<int32_t>("int32", n, AbsSum<int32_t>, SSEAbsSum32, Info, Results);
        RunAbsSum<int64_t>("int64", n, AbsSum<int64_t>, SSEAbsSum64, Info, Results);
        RunAbsSum<float>("float", n, AbsSum<float>, SSEAbsSumF, Info, Results, AVXAbsSumF, Info.avx);
        RunAbsSum<double>("double", n, AbsSum<double>, SSEAbsSumD, Info, Results, AVXAbsSumD, Info.avx);

        PrintResults(Results, n);
    }


    cout << "\n\n\n" << endl;
    cout << "Task 9 " << separator << endl << endl;

    RunShift(Info);
    cout << "\n\n\n" << endl;



    TestTask67();
    TestTask8();
    system("pause");

    return 0;
}