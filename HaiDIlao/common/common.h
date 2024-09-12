#pragma once
#include <iostream>
#include <string>
#include <vector>

#include <windows.h>

#include <Psapi.h>
#include <tchar.h>
#include <tlhelp32.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <functional>



#define EXPORT_API extern "C"

#define DLL_EXPORT_API  extern "C" __declspec(dllexport) 



#ifndef ARRAY_COUNT
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define STRINGIZE2(a) #a
#define STRINGIZE(a) STRINGIZE2(a)

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#define EraseMem(a, b) memset(a, 0, b)
#define EraseEl(a) memset((void *)&a, 0, sizeof(a))

#define ENABLED_LOG 1

template <typename T>
T CLAMP(const T& val, const T& mn, const T& mx)
{
    return val < mn ? mn : (val > mx ? mx : val);
}

template <typename T>
T MIN(const T& a, const T& b)
{
    return a < b ? a : b;
}

template <typename T>
T MAX(const T& a, const T& b)
{
    return a > b ? a : b;
}

template <typename T>
T LERP(const T& a, const T& b, const T& step)
{
    return (1.0f - step) * a + step * b;
}

inline bool ISNAN(float input)
{
    union
    {
        uint32_t u;
        float f;
    } x;

    x.f = input;

    // ignore sign bit (0x80000000)
    //     check that exponent (0x7f800000) is fully set
    // AND that mantissa (0x007fffff) is greater than 0 (if it's 0 then this is an inf)
    return (x.u & 0x7fffffffU) > 0x7f800000U;
}

inline bool ISINF(float input)
{
    union
    {
        uint32_t u;
        float f;
    } x;

    x.f = input;

    // ignore sign bit (0x80000000)
    //     check that exponent (0x7f800000) is fully set
    // AND that mantissa (0x007fffff) is exactly than 0 (if it's non-0 then this is an nan)
    return (x.u & 0x7fffffffU) == 0x7f800000U;
}

inline bool ISFINITE(float input)
{
    union
    {
        uint32_t u;
        float f;
    } x;

    x.f = input;

    // ignore sign bit (0x80000000)
    //     check that exponent (0x7f800000) is not fully set (if it's fully set then this is a
    //     nan/inf)
    return (x.u & 0x7f800000U) != 0x7f800000U;
}
