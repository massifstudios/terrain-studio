#pragma once

#include <cstdint>
#include <string>

namespace terrain::gpu {

/// Which routine a parity run exercises.
enum class ArithmeticRoutine {
    Reciprocal = 0,
    SquareRoot = 1,
    Divide = 2,
    RoundToInt = 3
};

struct ParityRequest {
    ArithmeticRoutine routine = ArithmeticRoutine::Reciprocal;
    /// Sweeps bit patterns upwards from `firstPattern`. Reciprocal and square
    /// root take a single operand, so a sweep of 2^32 is exhaustive: a proof
    /// rather than a sample. Division takes two and cannot be swept.
    bool isSequential = true;
    std::uint64_t firstPattern = 0;
    std::uint64_t count = 0;
    /// Draws the operands a sweep does not supply.
    std::uint64_t seed = 0;
};

struct ParityReport {
    bool ranOnDevice = false;
    /// Inputs whose answer the erosion solver could actually meet. Patterns
    /// outside that domain are skipped rather than counted.
    std::uint64_t testedCount = 0;
    std::uint64_t mismatchCount = 0;
    std::uint32_t firstMismatchPattern = 0;
    std::uint32_t hostResult = 0;
    std::uint32_t deviceResult = 0;
};

/// Compares the device's answer with the host's, bit for bit, over the
/// requested inputs.
ParityReport checkArithmeticParity(const ParityRequest& request);

/**
 * Whether this machine's chosen device reproduces the host's arithmetic.
 *
 * Answered once per process against a fixed pseudo-random sample of a few
 * million inputs per routine — the exhaustive sweep belongs in the test suite,
 * not in the startup path of an interactive tool — and cached. Erosion asks
 * this before it offers to run on the GPU, because no vendor exposes control
 * over 32-bit denormal handling and a capability bit therefore cannot answer
 * it.
 */
bool deviceMatchesHostArithmetic(std::string* failureDetail);

}  // namespace terrain::gpu
