#ifndef TERRAIN_EXACT_MATH_GLSL
#define TERRAIN_EXACT_MATH_GLSL

/*
 * Division and square root that agree with the CPU bit for bit.
 *
 * Vulkan only promises correct rounding for add, subtract, multiply and fused
 * multiply-add. Its 32-bit divide is allowed 2.5 units in the last place and
 * its square root 3.0, so writing `a / b` in a shader and `a / b` in C++ gives
 * two different terrains — which the determinism contract forbids.
 *
 * Both routines below are built from the four operations that are correctly
 * rounded, and both end in the same way: form the exact residual of a nearly
 * right answer with an fma, then fold it back in. That last step is what turns
 * an approximation into the correctly rounded result, and it is why the
 * reciprocal is refined first — the final correction is only exact when the
 * value it is scaled by is itself correctly rounded.
 */

const float smallestNormalFloat = 1.17549435e-38;

/// 1 / value, correctly rounded. Two Newton steps from the hardware's
/// approximation; each one squares the relative error, so 2.5 ulp becomes
/// exact well before the second step finishes.
precise float terrainReciprocal(float value) {
    precise float estimate = 1.0 / value;
    precise float error = fma(-value, estimate, 1.0);
    estimate = fma(estimate, error, estimate);
    error = fma(-value, estimate, 1.0);
    return fma(estimate, error, estimate);
}

/*
 * Both routines below work on mantissas rather than on the values themselves.
 *
 * The correction that makes them exact is an fma-formed residual, which is by
 * construction about one unit in the last place of its operands — so for an
 * operand near the bottom of the exponent range that residual is itself
 * denormal, and a device that flushes denormals to zero throws the correction
 * away and lands one ulp out. Splitting off the exponent first puts every
 * mantissa in [0.5, 1), where the residual has nowhere near that far to fall.
 * Both the split and the reassembly are exact: a power of two never rounds.
 */

/// numerator / denominator for mantissas, where neither can be denormal.
precise float terrainDivideMantissas(float numerator, float denominator) {
    precise float reciprocal = terrainReciprocal(denominator);
    precise float quotient = numerator * reciprocal;
    precise float residual = fma(-denominator, quotient, numerator);
    return fma(residual, reciprocal, quotient);
}

/// numerator / denominator, correctly rounded.
precise float terrainDivide(float numerator, float denominator) {
    if (isnan(numerator) || isnan(denominator) || isinf(numerator) || isinf(denominator)
        || denominator == 0.0 || numerator == 0.0) {
        return numerator / denominator;
    }
    int numeratorExponent = 0;
    int denominatorExponent = 0;
    precise float numeratorMantissa = frexp(numerator, numeratorExponent);
    precise float denominatorMantissa = frexp(denominator, denominatorExponent);
    precise float quotient = terrainDivideMantissas(numeratorMantissa, denominatorMantissa);
    return ldexp(quotient, numeratorExponent - denominatorExponent);
}

/// sqrt(value) for a mantissa in [0.25, 1).
precise float terrainSquareRootOfMantissa(float value) {
    precise float inverseRoot = inversesqrt(value);
    precise float root = value * inverseRoot;
    precise float halfInverseRoot = 0.5 * inverseRoot;

    precise float correction = fma(-root, halfInverseRoot, 0.5);
    root = fma(root, correction, root);
    halfInverseRoot = fma(halfInverseRoot, correction, halfInverseRoot);

    correction = fma(-root, halfInverseRoot, 0.5);
    root = fma(root, correction, root);
    halfInverseRoot = fma(halfInverseRoot, correction, halfInverseRoot);

    precise float residual = fma(-root, root, value);
    return fma(residual, halfInverseRoot, root);
}

/// sqrt(value), correctly rounded.
///
/// The exponent is forced even before it is halved, since half an odd exponent
/// is not an integer and the leftover factor of two has to live in the mantissa
/// instead.
precise float terrainSquareRoot(float value) {
    if (value == 0.0 || isnan(value) || isinf(value) || value < 0.0) {
        return sqrt(value);
    }
    int exponent = 0;
    precise float mantissa = frexp(value, exponent);
    if ((exponent & 1) != 0) {
        mantissa = mantissa * 0.5;
        exponent = exponent + 1;
    }
    return ldexp(terrainSquareRootOfMantissa(mantissa), exponent / 2);
}

/// The C library's `lround`: halfway cases go away from zero, which is not
/// what GLSL's `round` promises and not what `roundEven` does.
int terrainRoundToInt(float value) {
    precise float whole = trunc(value);
    precise float fraction = value - whole;
    if (fraction >= 0.5) {
        return int(whole) + 1;
    }
    if (fraction <= -0.5) {
        return int(whole) - 1;
    }
    return int(whole);
}

#endif
