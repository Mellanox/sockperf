/*
 * Copyright (c) 2011-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/**
 * @file ticks_os.h
 *
 *
 * @details: Base functions taken from ticks.h in favor of Windows compatibility,
 * using QueryPerformanceFrequency() and QueryPerformanceCounter() for max accuracy.
 *
 *
 * @author  Meny Yossefi <menyy@mellanox.com>
 *
 **/
#ifndef _TICKS_OS_H_
#define _TICKS_OS_H_

#include <time.h> /* clock_gettime()*/
#include <exception>
#include <stdint.h> // for int64_t
#include <stdlib.h> // for qsort
#include "clock.h"

typedef int64_t ticks_t;

#ifdef __windows__

#include <WinSock2.h>
#include <Winbase.h>

#pragma warning(disable : 4290) // exception specification ignored except to indicate a function is
                                // not __declspec(nothrow)
#pragma warning(disable                                                                            \
                : 4996) // The compiler encountered a function that was marked with deprecated.

#define usleep(x) Sleep(x / 1000)
#define snprintf _snprintf

#endif

/**
 * RDTSC extensions
 */
#define TSCVAL_INITIALIZER (0)

static const int64_t NSEC_IN_SEC = 1000 * 1000 * 1000;
// Utility function
inline ticks_t timespec2nsec(const struct timespec &_val) {
    return NSEC_IN_SEC * _val.tv_sec + _val.tv_nsec;
}

inline ticks_t os_gettimeoftsc() {
#ifdef __windows__
    LARGE_INTEGER lp;
    double PCFreq = 0.0;
    QueryPerformanceFrequency(&lp);
    PCFreq = (double(lp.QuadPart)) / (double)NSEC_IN_SEC; // NanoSec
    QueryPerformanceCounter(&lp);
    lp.QuadPart = (LONGLONG)((lp.QuadPart) / (PCFreq));
    return (ticks_t)(lp.QuadPart);
#else
    register uint32_t upper_32, lower_32;

#if defined(__powerpc64__)
    unsigned long long ret;
    asm volatile("mftb %0" : "=r"(ret) :);
    return (ticks_t)ret;
#elif defined(__s390__)
    unsigned long long ret;
    asm volatile("stck %0" : "=Q"(ret) : : "cc");
    return (ticks_t)ret;
#elif defined(__aarch64__)
    uint64_t ret;
    asm volatile("isb" : : : "memory");
    asm volatile("mrs %0, cntvct_el0" : "=r" (ret));
    return ret;
#else
    // ReaD Time Stamp Counter (RDTCS)
    __asm__ __volatile__("rdtsc" : "=a"(lower_32), "=d"(upper_32));
#endif
    // Return to user
    return (((ticks_t)upper_32) << 32) | lower_32;
#endif
}

inline void os_ts_gettimeofclock(struct timespec *pts) {
#ifdef __windows__
    ticks_t val = os_gettimeoftsc(); // probably just NSEC_IN_SEC
    pts->tv_sec = val / NSEC_IN_SEC;
    pts->tv_nsec = val % NSEC_IN_SEC;
#else

    if (clock_gettime(CLOCK_MONOTONIC, pts)) {
        throw("clock_gettime failed");
    }
#endif
}

inline ticks_t os_gettimeofclock() {
    struct timespec ts;
    os_ts_gettimeofclock(&ts);
    return timespec2nsec(ts);
}

#endif /*_TICKS_OS_H_*/
