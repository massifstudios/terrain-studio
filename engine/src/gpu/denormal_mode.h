#pragma once

namespace terrain::gpu {

/**
 * Puts the calling thread into flush-to-zero for the duration of a scope.
 *
 * Erosion has to give the same answer on the CPU and on the GPU, and denormals
 * are where the two disagree. No vendor offers control over 32-bit denormal
 * handling on every device — NVIDIA offers none at all and flushes them — so
 * the choice is not which behaviour to standardise on but whose behaviour the
 * other has to adopt. The host adopts the device's.
 *
 * What this costs is precision below 1.2e-38, in a solver whose quantities are
 * heights in meters and weights in [0, 1]. What it buys is that the contract
 * holds.
 *
 * The mode is a thread's own, so every worker a node runs has to set it, and
 * the destructor puts back exactly what it found rather than assuming the
 * thread started in the default.
 */
class FlushDenormalsToZero {
public:
    FlushDenormalsToZero();
    ~FlushDenormalsToZero();

    FlushDenormalsToZero(const FlushDenormalsToZero&) = delete;
    FlushDenormalsToZero& operator=(const FlushDenormalsToZero&) = delete;

    /// False where the platform has no such control, in which case the GPU
    /// backends refuse themselves rather than produce a second answer.
    static bool isSupported();

private:
    /// Wide enough for AArch64's FPCR as well as x86's MXCSR.
    unsigned long long previousState_ = 0;
};

}  // namespace terrain::gpu
