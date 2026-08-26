#include "denormal_mode.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define TERRAIN_HAS_MXCSR 1
#include <pmmintrin.h>
#include <xmmintrin.h>
#elif defined(__aarch64__)
#define TERRAIN_HAS_FPCR 1
#endif

namespace terrain::gpu {
namespace {

#if defined(TERRAIN_HAS_FPCR)
/// Bit 24 of the AArch64 floating-point control register is Flush-to-Zero,
/// which on that architecture covers both operands and results.
constexpr unsigned long long flushToZeroBit = 1ull << 24;

unsigned long long readControlRegister() {
    unsigned long long value = 0;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(value));
    return value;
}

void writeControlRegister(unsigned long long value) {
    __asm__ __volatile__("msr fpcr, %0" : : "r"(value));
}
#endif

}  // namespace

#if defined(TERRAIN_HAS_MXCSR)

FlushDenormalsToZero::FlushDenormalsToZero() {
    const unsigned int previous = _mm_getcsr();
    previousState_ = previous;
    // Flush-to-zero covers results, denormals-are-zero covers operands. A
    // device that flushes does both, so setting only one would leave a gap.
    _mm_setcsr(previous | _MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON);
}

FlushDenormalsToZero::~FlushDenormalsToZero() {
    _mm_setcsr(static_cast<unsigned int>(previousState_));
}

bool FlushDenormalsToZero::isSupported() {
    return true;
}

#elif defined(TERRAIN_HAS_FPCR)

FlushDenormalsToZero::FlushDenormalsToZero() {
    const unsigned long long previous = readControlRegister();
    previousState_ = previous;
    writeControlRegister(previous | flushToZeroBit);
}

FlushDenormalsToZero::~FlushDenormalsToZero() {
    writeControlRegister(previousState_);
}

bool FlushDenormalsToZero::isSupported() {
    return true;
}

#else

FlushDenormalsToZero::FlushDenormalsToZero() = default;
FlushDenormalsToZero::~FlushDenormalsToZero() = default;

bool FlushDenormalsToZero::isSupported() {
    return false;
}

#endif

}  // namespace terrain::gpu
