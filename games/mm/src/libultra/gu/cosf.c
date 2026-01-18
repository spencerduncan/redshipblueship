#include "ultra64.h"
#include "libc/math.h"
#include "ultra64.h"
#include "z64math.h"

#pragma weak cosf = __cosf

// Coefficients of a degree 9 polynomial approximation of sine. It is not the Maclaurin polynamial, but some as-yet
// undetermined more uniform approximation.
static const du MM_P[] = {
    { 1.0 },
    { -0.16666659550427756 },
    { 0.008333066246082155 },
    { -0.0001980960290193795 },
    { 0.000002605780637968037 },
};

static const du MM_rpi = { 1 / 3.14159265358979323846 }; // 1/M_PI, "reciprocal of pi"

// MM_pihi + MM_pilo is the closest double to pi, this representation allows more precise calculations since pi itself is not
// an exact float
static const du MM_pihi = { 3.1415926218032837 };

static const du MM_pilo = { 3.178650954705639E-8 };

static const fu MM_zero = { 0x00000000 };

/**
 * Computes the cosine of a float, returning a float. It essentially computes sin(x+pi/2) by the same method as __sinf,
 * without as many size checks.
 */
f32 __cosf(f32 x) {
    f32 absx;
    f64 dx;         // x promoted to double
    f64 xSq;        // square of dx
    f64 polyApprox; // Most of the polynomial approximation to sin(x)
    f64 dn;         // n promoted to double
    s32 n;          // number of multiples of pi away from the first half-period
    f64 result;
    s32 ix = *(s32*)&x;   // Type-pun x into an s32, i.e. its IEEE-754 hex representation
    s32 xpt = (ix >> 22); // Obtain the exponent of x (actually 2 * exponent + 127)

    xpt &= 0x1FF; // Remove the sign bit

    // |x| < 2^{28} (beyond this range, floats are too sparse to make the trig functions usable)
    if (xpt < 310) {
        absx = (x > 0) ? x : -x;
        dx = absx;

        dn = dx * MM_rpi.d + 0.5;
        n = ROUND(dn);
        dn = n;

        dn -= 0.5;

        dx -= dn * MM_pihi.d;
        dx -= dn * MM_pilo.d;

        xSq = dx * dx;
        polyApprox = ((MM_P[4].d * xSq + MM_P[3].d) * xSq + MM_P[2].d) * xSq + MM_P[1].d;
        result = dx + (dx * xSq) * polyApprox; // Actual Maclaurin polynomial for sin(x)

        if (n % 2 == 0) {
            return (f32)result;
        }
        return -(f32)result;
    }

    // if x is NaN
    if (x != x) {
        return __libm_qnan_f;
    }

    return MM_zero.f;
}
