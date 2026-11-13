#include "TasksPartTwo.h"

#include <intrin.h>
#include <immintrin.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

namespace {

// "Порівняти обчислювальну складність для цих функцій для довжини масиву 4096"
const size_t ARRAY_SIZE = 4096;

const size_t FLOATS_PER_SSE  = sizeof(__m128) / sizeof(float);
const size_t DOUBLES_PER_SSE = sizeof(__m128d) / sizeof(double);
const size_t FLOATS_PER_AVX  = sizeof(__m256) / sizeof(float);
const size_t DOUBLES_PER_AVX = sizeof(__m256d) / sizeof(double);



// Task 6. x[i] = sqrt(y[i]) -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

__declspec(noinline) void SqrtF(const float* y, float* x, size_t n) {
#pragma loop(no_vector)
    for (size_t i = 0; i < n; ++i) {
        x[i] = sqrtf(y[i]);
    }
}

__declspec(noinline) void SqrtD(const double* y, double* x, size_t n) {
#pragma loop(no_vector)
    for (size_t i = 0; i < n; ++i) {
        x[i] = sqrt(y[i]);
    }
}

__declspec(noinline) void SSESqrtF(const float* y, float* x, size_t n) {
    size_t i;
    const __m128* py = (const __m128*)y;
    __m128* px = (__m128*)x;
    for (i = 0; i < n / FLOATS_PER_SSE; ++i)
        px[i] = _mm_sqrt_ps(py[i]);

    size_t Done = i * FLOATS_PER_SSE;
    SqrtF(y + Done, x + Done, n - Done);
}

__declspec(noinline) void SSESqrtD(const double* y, double* x, size_t n) {
    size_t i;
    const __m128d* py = (const __m128d*)y;
    __m128d* px = (__m128d*)x;
    for (i = 0; i < n / DOUBLES_PER_SSE; ++i)
        px[i] = _mm_sqrt_pd(py[i]);

    size_t Done = i * DOUBLES_PER_SSE;
    SqrtD(y + Done, x + Done, n - Done);
}

__declspec(noinline) void AVXSqrtF(const float* y, float* x, size_t n) {
    size_t i;
    const __m256* py = (const __m256*)y;
    __m256* px = (__m256*)x;
    for (i = 0; i < n / FLOATS_PER_AVX; ++i)
        px[i] = _mm256_sqrt_ps(py[i]);

    size_t Done = i * FLOATS_PER_AVX;
    SqrtF(y + Done, x + Done, n - Done);
}

__declspec(noinline) void AVXSqrtD(const double* y, double* x, size_t n) {
    size_t i;
    const __m256d* py = (const __m256d*)y;
    __m256d* px = (__m256d*)x;
    for (i = 0; i < n / DOUBLES_PER_AVX; ++i)
        px[i] = _mm256_sqrt_pd(py[i]);

    size_t Done = i * DOUBLES_PER_AVX;
    SqrtD(y + Done, x + Done, n - Done);
}



// Task 8. x[i] = y[i] * z[i] for complex numbers -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//   (a + bi)(c + di) = (ac - bd) + (ad + bc)i

struct ComplexF {
    float re;
    float im;
};

const size_t COMPLEX_PER_SSE = sizeof(__m128) / sizeof(ComplexF);
const size_t COMPLEX_PER_AVX = sizeof(__m256) / sizeof(ComplexF);

__declspec(noinline) void MulComplex(const ComplexF* y, const ComplexF* z, ComplexF* x, size_t n) {
#pragma loop(no_vector)
    for (size_t i = 0; i < n; ++i) {
        float a = y[i].re;
        float b = y[i].im;
        float c = z[i].re;
        float d = z[i].im;
        x[i].re = a * c - b * d;
        x[i].im = a * d + b * c;
    }
}

// One register holds 2 complex numbers:
//   y              = [a0, b0, a1, b1]
//   z              = [c0, d0, c1, d1]
//   zRe            = [c0, c0, c1, c1]
//   zIm            = [d0, d0, d1, d1]
//   ySwapped       = [b0, a0, b1, a1]
//   y * zRe        = [a0c0,  b0c0, a1c1,  b1c1]
//   ySwapped * zIm = [b0d0,  a0d0, b1d1,  a1d1]  -> negate lanes 0 and 2
//   result         = [a0c0 - b0d0, b0c0 + a0d0, a1c1 - b1d1, b1c1 + a1d1]
__declspec(noinline) void SSEMulComplex(const ComplexF* y, const ComplexF* z, ComplexF* x, size_t n) {
    size_t i;
    const __m128* py = (const __m128*)y;
    const __m128* pz = (const __m128*)z;
    __m128* px = (__m128*)x;

    const __m128 NegateRe = _mm_setr_ps(-0.0f, 0.0f, -0.0f, 0.0f);

    for (i = 0; i < n / COMPLEX_PER_SSE; ++i) {
        __m128 yv = py[i];
        __m128 zv = pz[i];

        __m128 zRe      = _mm_shuffle_ps(zv, zv, _MM_SHUFFLE(2, 2, 0, 0));
        __m128 zIm      = _mm_shuffle_ps(zv, zv, _MM_SHUFFLE(3, 3, 1, 1));
        __m128 ySwapped = _mm_shuffle_ps(yv, yv, _MM_SHUFFLE(2, 3, 0, 1));

        __m128 First  = _mm_mul_ps(yv, zRe);
        __m128 Second = _mm_xor_ps(_mm_mul_ps(ySwapped, zIm), NegateRe);

        px[i] = _mm_add_ps(First, Second);
    }

    size_t Done = i * COMPLEX_PER_SSE;
    MulComplex(y + Done, z + Done, x + Done, n - Done);
}

// Same scheme as SSEMulComplex, 4 complex numbers per register
// (_mm256_shuffle_ps works inside each 128-bit half, so the masks are the same)
__declspec(noinline) void AVXMulComplex(const ComplexF* y, const ComplexF* z, ComplexF* x, size_t n) {
    size_t i;
    const __m256* py = (const __m256*)y;
    const __m256* pz = (const __m256*)z;
    __m256* px = (__m256*)x;

    const __m256 NegateRe = _mm256_setr_ps(-0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f);

    for (i = 0; i < n / COMPLEX_PER_AVX; ++i) {
        __m256 yv = py[i];
        __m256 zv = pz[i];

        __m256 zRe      = _mm256_shuffle_ps(zv, zv, _MM_SHUFFLE(2, 2, 0, 0));
        __m256 zIm      = _mm256_shuffle_ps(zv, zv, _MM_SHUFFLE(3, 3, 1, 1));
        __m256 ySwapped = _mm256_shuffle_ps(yv, yv, _MM_SHUFFLE(2, 3, 0, 1));

        __m256 First  = _mm256_mul_ps(yv, zRe);
        __m256 Second = _mm256_xor_ps(_mm256_mul_ps(ySwapped, zIm), NegateRe);

        px[i] = _mm256_add_ps(First, Second);
    }

    size_t Done = i * COMPLEX_PER_AVX;
    MulComplex(y + Done, z + Done, x + Done, n - Done);
}



// Verification -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
template <typename T>
bool AreEqual(const T* Expected, const T* Actual, size_t n) {
    const double RelativeTolerance = 1e-5;
    for (size_t i = 0; i < n; ++i) {
        double Difference = fabs((double)Expected[i] - (double)Actual[i]);
        if (Difference > RelativeTolerance * fabs((double)Expected[i])) {
            return false;
        }
    }
    return true;
}

bool AreEqual(const ComplexF* Expected, const ComplexF* Actual, size_t n) {
    return AreEqual((const float*)Expected, (const float*)Actual, 2 * n);
}



// Running -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
template <typename T>
using SqrtFunction = void (*)(const T*, T*, size_t);

template <typename T>
void RunSqrtType(const char* TypeName, size_t n, vector<Result>& Results,
    SqrtFunction<T> ScalarFunction,
    SqrtFunction<T> SSEFunction, bool UseSSE,
    SqrtFunction<T> AVXFunction, bool UseAVX) {

    size_t Bytes = n * sizeof(T);
    T* y = (T*)AllocateAligned(Bytes);
    T* Reference = (T*)AllocateAligned(Bytes);
    T* x = (T*)AllocateAligned(Bytes);

    if (y == NULL || Reference == NULL || x == NULL) {
        cout << "Not enough memory: " << TypeName << ", size = " << n << endl;
        FreeAligned(y);
        FreeAligned(Reference);
        FreeAligned(x);
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        y[i] = (T)RandomFloat(0.0f, 1000.0f);
    }

    size_t RepeatCount = RepeatCountForSize(n);

    Result Res;
    Res.Type = TypeName;
    Res.Size = n;
    Res.SSETime = -1;
    Res.AVXTime = -1;
    bool Correct = true;

    Res.ScalarTime = MeasureSeconds([&] { ScalarFunction(y, Reference, n); }, RepeatCount);

    if (UseSSE) {
        memset(x, 0, Bytes);
        Res.SSETime = MeasureSeconds([&] { SSEFunction(y, x, n); }, RepeatCount);
        Correct = Correct && AreEqual(Reference, x, n);
    }

    if (UseAVX) {
        memset(x, 0, Bytes);
        Res.AVXTime = MeasureSeconds([&] { AVXFunction(y, x, n); }, RepeatCount);
        Correct = Correct && AreEqual(Reference, x, n);
    }

    if (!Correct) {
        cout << "Wrong result: " << TypeName << ", size = " << n << endl;
    }
    Results.push_back(Res);

    FreeAligned(y);
    FreeAligned(Reference);
    FreeAligned(x);
}

}



void RunSqrt(const SIMDInfo& Info) {
    cout << "Task 6, 7: x[i] = sqrt(y[i])\n\n";

    vector<Result> Results;
    RunSqrtType<float>("float", ARRAY_SIZE, Results, SqrtF, SSESqrtF, Info.sse, AVXSqrtF, Info.avx);
    RunSqrtType<double>("double", ARRAY_SIZE, Results, SqrtD, SSESqrtD, Info.sse2, AVXSqrtD, Info.avx);

    PrintResults(Results, ARRAY_SIZE);
}

void RunComplexMultiply(const SIMDInfo& Info) {
    cout << "Task 8: x[i] = y[i] * z[i], complex float\n\n";

    const size_t n = ARRAY_SIZE;
    size_t Bytes = n * sizeof(ComplexF);
    ComplexF* y = (ComplexF*)AllocateAligned(Bytes);
    ComplexF* z = (ComplexF*)AllocateAligned(Bytes);
    ComplexF* Reference = (ComplexF*)AllocateAligned(Bytes);
    ComplexF* x = (ComplexF*)AllocateAligned(Bytes);

    if (y == NULL || z == NULL || Reference == NULL || x == NULL) {
        cout << "Not enough memory: complex, size = " << n << endl;
        FreeAligned(y);
        FreeAligned(z);
        FreeAligned(Reference);
        FreeAligned(x);
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        y[i] = { RandomFloat(-100.0f, 100.0f), RandomFloat(-100.0f, 100.0f) };
        z[i] = { RandomFloat(-100.0f, 100.0f), RandomFloat(-100.0f, 100.0f) };
    }

    size_t RepeatCount = RepeatCountForSize(n);

    Result Res;
    Res.Type = "complex";
    Res.Size = n;
    Res.SSETime = -1;
    Res.AVXTime = -1;
    bool Correct = true;

    Res.ScalarTime = MeasureSeconds([&] { MulComplex(y, z, Reference, n); }, RepeatCount);

    if (Info.sse) {
        memset(x, 0, Bytes);
        Res.SSETime = MeasureSeconds([&] { SSEMulComplex(y, z, x, n); }, RepeatCount);
        Correct = Correct && AreEqual(Reference, x, n);
    }

    if (Info.avx) {
        memset(x, 0, Bytes);
        Res.AVXTime = MeasureSeconds([&] { AVXMulComplex(y, z, x, n); }, RepeatCount);
        Correct = Correct && AreEqual(Reference, x, n);
    }

    if (!Correct) {
        cout << "Wrong result: complex, size = " << n << endl;
    }

    vector<Result> Results;
    Results.push_back(Res);
    PrintResults(Results, n);

    FreeAligned(y);
    FreeAligned(z);
    FreeAligned(Reference);
    FreeAligned(x);
}
