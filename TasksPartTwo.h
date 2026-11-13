#pragma once

#include "Common.h"

// Tasks 6-7: x[i] = sqrt(y[i]) for float and double - scalar / SSE / AVX
void RunSqrt(const SIMDInfo& Info);

// Task 8: x[i] = y[i] * z[i] for complex float - scalar / SSE / AVX
void RunComplexMultiply(const SIMDInfo& Info);
