#include <hardware.h>
#include <nitro/fx.h>

// Shift division result by this much after fx32 division, to get fx32 result
#define FX_DIV_SHIFT (32 - FX32B_INT)

// The value 1 in shifted divion result units
#define FX_DIV_ONE (1 << (FX_DIV_SHIFT))

// The value 0.5 in shifted division result units
#define FX_DIV_HALF (FX_DIV_ONE >> 1)

// Start fixed-point division using math coprocessor
void FX_divAsync(fx32 numer, fx32 denom) {
    *rDIVCNT = DIVCNT_MODE_64_32;

    // Shift numerator by 32 to avoid precision loss
    *rDIV_NUMER = (u64)numer << 32;
    *rDIV_DENOM = (u32)denom;
}

// Get result of fixed-point division from math coprocessor
fx32 FX_getDivResult() {
    while (*rDIVCNT & DIVCNTF_BUSY) {}

    // Shift result back to account for precision loss prevention.
    // Also add a half, to prevent rouding errors.
    return (*rDIV_RESULT + FX_DIV_HALF) >> FX_DIV_SHIFT;
}

// Get result of fixed-point division from math coprocessor
fx64c FX_getDivResultFx64c() {
    while (*rDIVCNT & DIVCNTF_BUSY) {}
    return *rDIV_RESULT;
}

// Fixed-point division using math coprocessor
fx32 FX_div(fx32 numer, fx32 denom) {
    FX_divAsync(numer, denom);
    return FX_getDivResult();
}

// Signed integer modulo using math coprocessor
s32 FX_modS32(s32 numer, s32 denom) {
    *rDIVCNT = DIVCNT_MODE_32_32;
    *rDIV_NUMER_L = (u32)numer;
    *rDIV_DENOM = (u32)denom;

    while (*rDIVCNT & DIVCNTF_BUSY) {}
    return *rDIVREM_RESULT_L;
}



// Shift sqrt result by this much after fx32 square root, to get fx32 result
#define FX_SQRT_SHIFT ((32 + FX32B_INT) / 2 - FX32B_INT)

// The value 1 in shifted square root result units
#define FX_SQRT_ONE (1 << (FX_SQRT_SHIFT))

// The value 0.5 in shifted square root result units
#define FX_SQRT_HALF (FX_SQRT_ONE >> 1)

// Start fixed-point square root using math coprocessor
void FX_sqrtAsync(fx32 x) {
    if (x > 0) {
        *rSQRTCNT = SQRTCNT_MODE_64;

        // Shift operand by 32 to avoid precision loss
        *rSQRT_PARAM = (u64)x << 32;
    }
    else {
        *rSQRTCNT = SQRTCNT_MODE_64;
        *rSQRT_PARAM = 0;
    }
}

// Get result of fixed-point square root from math coprocessor
fx32 FX_getSqrtResult(void) {
    while (*rSQRTCNT & SQRTCNTF_BUSY) {}

    // Shift result back to account for precision loss prevention.
    // Also add a half, to prevent rouding errors.
    return (*rSQRT_RESULT + FX_SQRT_HALF) >> FX_SQRT_SHIFT;
}

// Fixed-point square root using math coprocessor
fx32 FX_sqrt(fx32 x) {
    if (x <= 0) {
        return 0;
    }

    *rSQRTCNT = SQRTCNT_MODE_64;
    *rSQRT_PARAM = (u64)x << 32;
    return FX_getSqrtResult();
}



// Start calculating 1.0 / x using math coprocessor
void FX_invAsync(fx32 x) {
    *rDIVCNT = DIVCNT_MODE_64_32;
    *rDIV_NUMER = (u64)FX32_ONE << 32;
    *rDIV_DENOM = (u32)x;
}

// Calculate 1.0 / x using math coprocessor
fx32 FX_inv(fx32 x) {
    FX_invAsync(x);
    return FX_getDivResult();
}

// Calculate the inverse square root of x using math coprocessor
fx32 FX_invSqrt(fx32 x) {
    fx64c divResult;
    s64 sqrtResult;
    s64 invSqrtResult;

    if (x > 0) {
        FX_invAsync(x);
        FX_sqrtAsync(x);

        divResult = FX_getDivResultFx64c();
        while (*rSQRTCNT & SQRTCNTF_BUSY) {}
        sqrtResult = *(s32*)rSQRT_RESULT;

        // Shift result back to account for precision loss prevention.
        // Also add a half, to prevent rouding errors.
        invSqrtResult = divResult * sqrtResult;
        return (invSqrtResult + FX_SQRT_HALF) >> FX_SQRT_SHIFT;
    }
    else {
        return 0;
    }
}
