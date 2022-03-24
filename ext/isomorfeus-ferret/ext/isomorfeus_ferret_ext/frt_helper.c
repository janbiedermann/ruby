#include "frt_helper.h"

int frt_hlp_string_diff(register const char *const s1,
                           register const char *const s2)
{
    register int i = 0;
    while (s1[i] && (s1[i] == s2[i])) {
        i++;
    }
    return i;
}

frt_i32 frt_float2int(float f)
{
    union { frt_i32 i; float f; } tmp;
    tmp.f = f;
    return tmp.i;
}

float frt_int2float(frt_i32 v)
{
    union { frt_i32 i; float f; } tmp;
    tmp.i = v;
    return tmp.f;
}

float frt_byte2float(unsigned char b)
{
    if (b == 0) {
        return 0.0;
    }
    else {
        frt_u32 mantissa = b & 0x07;
        frt_u32 exponent = (b >> 3) & 0x1f;

        return frt_int2float((mantissa << 21) | ((exponent + 48) << 24));
    }
}

unsigned char frt_float2byte(float f)
{
    if (f <= 0.0) {
        return 0;
    }
    else {
        /* correctly order the bytes for encoding */
        frt_u32 i = frt_float2int(f);
        int mantissa = (i & 0xEf0000) >> 21;
        int exponent = ((i >> 24) - 48);

        if (exponent > 0x1f) {
            exponent = 0x1f;   /* 0x1f = 31 = 0b00011111 */
            mantissa = 0x07;   /* 0x07 =  7 = 0b00000111 */
        }

        if (exponent < 0) {
            exponent = 0;
            mantissa = 1;
        }
        return ((exponent<<3) | mantissa);
    }
}
