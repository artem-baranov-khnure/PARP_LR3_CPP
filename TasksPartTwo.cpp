#include "TasksPartTwo.h"

#include <intrin.h>
#include <malloc.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace {

const size_t ARRAY_SIZE  = 4096;
const int    REPETITIONS = 10000;
const size_t SSE_ALIGNMENT = 16;

const size_t FLOATS_PER_SSE    = sizeof(__m128) / sizeof(float);
const size_t DOUBLES_PER_SSE   = sizeof(__m128d) / sizeof(double);


// Processor check

const int CPUID_FEATURES = 1;
const int EDX_SSE_BIT    = 25;
const int EDX_SSE2_BIT   = 26;

bool IsBitSet(int value, int bit) {
    return ((value >> bit) & 1) != 0;
}

bool IsSSE2Supported() {
    int registers[4];  // EAX, EBX, ECX, EDX
    __cpuid(registers, CPUID_FEATURES);
    int edx = registers[3];
    return IsBitSet(edx, EDX_SSE_BIT) && IsBitSet(edx, EDX_SSE2_BIT);
}


// Aligned memory

struct AlignedDeleter {
    void operator()(void* pointer) const {
        _aligned_free(pointer);
    }
};

template <typename T>
using AlignedArray = std::unique_ptr<T[], AlignedDeleter>;

template <typename T>
AlignedArray<T> AllocateAligned(size_t count) {
    void* memory = _aligned_malloc(count * sizeof(T), SSE_ALIGNMENT);
    return AlignedArray<T>(static_cast<T*>(memory));
}


// Task 6. x[i] = sqrt(y[i])

__declspec(noinline) void SqrtFloatScalar(float* x, const float* y, size_t count) {
#pragma loop(no_vector)
    for (size_t i = 0; i < count; ++i) {
        x[i] = std::sqrt(y[i]);
    }
}

__declspec(noinline) void SqrtFloatSSE(float* x, const float* y, size_t count) {
    const __m128* yBlocks = reinterpret_cast<const __m128*>(y);
    __m128*       xBlocks = reinterpret_cast<__m128*>(x);
    size_t blockCount = count / FLOATS_PER_SSE;

    for (size_t i = 0; i < blockCount; ++i) {
        xBlocks[i] = _mm_sqrt_ps(yBlocks[i]);
    }

    size_t processed = blockCount * FLOATS_PER_SSE;
    SqrtFloatScalar(x + processed, y + processed, count - processed);
}

__declspec(noinline) void SqrtDoubleScalar(double* x, const double* y, size_t count) {
#pragma loop(no_vector)
    for (size_t i = 0; i < count; ++i) {
        x[i] = std::sqrt(y[i]);
    }
}

__declspec(noinline) void SqrtDoubleSSE(double* x, const double* y, size_t count) {
    const __m128d* yBlocks = reinterpret_cast<const __m128d*>(y);
    __m128d*       xBlocks = reinterpret_cast<__m128d*>(x);
    size_t blockCount = count / DOUBLES_PER_SSE;

    for (size_t i = 0; i < blockCount; ++i) {
        xBlocks[i] = _mm_sqrt_pd(yBlocks[i]);
    }

    size_t processed = blockCount * DOUBLES_PER_SSE;
    SqrtDoubleScalar(x + processed, y + processed, count - processed);
}


// Task 8. x[i] = y[i] * z[i] for complex numbers
//   (a + bi)(c + di) = (ac - bd) + (ad + bc)i

struct ComplexF {
    float re;
    float im;
};

const size_t COMPLEX_PER_SSE = sizeof(__m128) / sizeof(ComplexF);

__declspec(noinline) void MultiplyComplexScalar(ComplexF* x, const ComplexF* y, const ComplexF* z, size_t count) {
#pragma loop(no_vector)
    for (size_t i = 0; i < count; ++i) {
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
__declspec(noinline) void MultiplyComplexSSE(ComplexF* x, const ComplexF* y, const ComplexF* z, size_t count) {
    const __m128* yBlocks = reinterpret_cast<const __m128*>(y);
    const __m128* zBlocks = reinterpret_cast<const __m128*>(z);
    __m128*       xBlocks = reinterpret_cast<__m128*>(x);
    size_t blockCount = count / COMPLEX_PER_SSE;

    const __m128 negateRealLanes = _mm_setr_ps(-0.0f, 0.0f, -0.0f, 0.0f);

    for (size_t i = 0; i < blockCount; ++i) {
        __m128 yBlock = yBlocks[i];
        __m128 zBlock = zBlocks[i];

        __m128 zRe      = _mm_shuffle_ps(zBlock, zBlock, _MM_SHUFFLE(2, 2, 0, 0));
        __m128 zIm      = _mm_shuffle_ps(zBlock, zBlock, _MM_SHUFFLE(3, 3, 1, 1));
        __m128 ySwapped = _mm_shuffle_ps(yBlock, yBlock, _MM_SHUFFLE(2, 3, 0, 1));

        __m128 first  = _mm_mul_ps(yBlock, zRe);
        __m128 second = _mm_mul_ps(ySwapped, zIm);
        second = _mm_xor_ps(second, negateRealLanes);

        xBlocks[i] = _mm_add_ps(first, second);
    }

    size_t processed = blockCount * COMPLEX_PER_SSE;
    MultiplyComplexScalar(x + processed, y + processed, z + processed, count - processed);
}


// Measurement and verification

template <typename Function>
double MeasureMicroseconds(Function function) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REPETITIONS; ++i) {
        function();
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::micro> total = end - start;
    return total.count() / REPETITIONS;
}

template <typename T>
bool AreEqual(const T* expected, const T* actual, size_t count) {
    const double relativeTolerance = 1e-5;
    for (size_t i = 0; i < count; ++i) {
        double difference = std::fabs(expected[i] - actual[i]);
        if (difference > relativeTolerance * std::fabs(expected[i])) {
            return false;
        }
    }
    return true;
}

bool AreEqual(const ComplexF* expected, const ComplexF* actual, size_t count) {
    const float* expectedComponents = reinterpret_cast<const float*>(expected);
    const float* actualComponents   = reinterpret_cast<const float*>(actual);
    return AreEqual(expectedComponents, actualComponents, 2 * count);
}

float RandomFloat(float min, float max) {
    float fraction = static_cast<float>(rand()) / RAND_MAX;
    return min + (max - min) * fraction;
}

ComplexF RandomComplex(float min, float max) {
    return { RandomFloat(min, max), RandomFloat(min, max) };
}


// Output

const int NAME_WIDTH    = 24;
const int TIME_WIDTH    = 16;
const int SPEEDUP_WIDTH = 10;

void PrintTableHeader(const std::string& title) {
    std::cout << '\n' << title << "  (N = " << ARRAY_SIZE << ", " << REPETITIONS << " calls)\n";

    std::cout << std::left  << std::setw(NAME_WIDTH)    << "Function"
              << std::right << std::setw(TIME_WIDTH)    << "Time/call, us"
              << std::right << std::setw(SPEEDUP_WIDTH) << "Speedup"
              << '\n';

    std::cout << std::string(NAME_WIDTH + TIME_WIDTH + SPEEDUP_WIDTH, '-') << '\n';
}

void PrintTableRow(const std::string& name, double microseconds, double baselineMicroseconds) {
    double speedup = baselineMicroseconds / microseconds;

    std::cout << std::fixed
              << std::left  << std::setw(NAME_WIDTH) << name
              << std::right << std::setw(TIME_WIDTH) << std::setprecision(3) << microseconds
              << std::right << std::setw(SPEEDUP_WIDTH - 1) << std::setprecision(2) << speedup << 'x'
              << '\n';
}

void PrintVerification(bool resultsMatch) {
    std::cout << "SSE result matches scalar: " << (resultsMatch ? "yes" : "NO") << '\n';
}

}

void TestTask67() {
    if (!IsSSE2Supported()) {
        std::cout << "SSE2 is not supported\n";
        return;
    }

    AlignedArray<float> yFloat         = AllocateAligned<float>(ARRAY_SIZE);
    AlignedArray<float> xFloatScalar   = AllocateAligned<float>(ARRAY_SIZE);
    AlignedArray<float> xFloatSSE      = AllocateAligned<float>(ARRAY_SIZE);
    AlignedArray<double> yDouble       = AllocateAligned<double>(ARRAY_SIZE);
    AlignedArray<double> xDoubleScalar = AllocateAligned<double>(ARRAY_SIZE);
    AlignedArray<double> xDoubleSSE    = AllocateAligned<double>(ARRAY_SIZE);

    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        yFloat[i]  = RandomFloat(0.0f, 1000.0f);
        yDouble[i] = RandomFloat(0.0f, 1000.0f);
    }

    double floatScalarTime = MeasureMicroseconds([&] {
        SqrtFloatScalar(xFloatScalar.get(), yFloat.get(), ARRAY_SIZE);
    });
    double floatSSETime = MeasureMicroseconds([&] {
        SqrtFloatSSE(xFloatSSE.get(), yFloat.get(), ARRAY_SIZE);
    });
    double doubleScalarTime = MeasureMicroseconds([&] {
        SqrtDoubleScalar(xDoubleScalar.get(), yDouble.get(), ARRAY_SIZE);
    });
    double doubleSSETime = MeasureMicroseconds([&] {
        SqrtDoubleSSE(xDoubleSSE.get(), yDouble.get(), ARRAY_SIZE);
    });

    PrintTableHeader("Tasks 6-7: x[i] = sqrt(y[i]), float");
    PrintTableRow("Scalar", floatScalarTime, floatScalarTime);
    PrintTableRow("SSE (sqrtps)", floatSSETime, floatScalarTime);
    PrintVerification(AreEqual(xFloatScalar.get(), xFloatSSE.get(), ARRAY_SIZE));

    PrintTableHeader("Tasks 6-7: x[i] = sqrt(y[i]), double");
    PrintTableRow("Scalar", doubleScalarTime, doubleScalarTime);
    PrintTableRow("SSE2 (sqrtpd)", doubleSSETime, doubleScalarTime);
    PrintVerification(AreEqual(xDoubleScalar.get(), xDoubleSSE.get(), ARRAY_SIZE));
}

void TestTask8() {
    if (!IsSSE2Supported()) {
        std::cout << "SSE2 is not supported\n";
        return;
    }

    AlignedArray<ComplexF> y       = AllocateAligned<ComplexF>(ARRAY_SIZE);
    AlignedArray<ComplexF> z       = AllocateAligned<ComplexF>(ARRAY_SIZE);
    AlignedArray<ComplexF> xScalar = AllocateAligned<ComplexF>(ARRAY_SIZE);
    AlignedArray<ComplexF> xSSE    = AllocateAligned<ComplexF>(ARRAY_SIZE);

    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        y[i] = RandomComplex(-100.0f, 100.0f);
        z[i] = RandomComplex(-100.0f, 100.0f);
    }

    double scalarTime = MeasureMicroseconds([&] {
        MultiplyComplexScalar(xScalar.get(), y.get(), z.get(), ARRAY_SIZE);
    });
    double sseTime = MeasureMicroseconds([&] {
        MultiplyComplexSSE(xSSE.get(), y.get(), z.get(), ARRAY_SIZE);
    });

    PrintTableHeader("Task 8: x[i] = y[i] * z[i], complex float");
    PrintTableRow("Scalar", scalarTime, scalarTime);
    PrintTableRow("SSE", sseTime, scalarTime);
    PrintVerification(AreEqual(xScalar.get(), xSSE.get(), ARRAY_SIZE));
}
