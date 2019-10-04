/*
 * Copyright (c) 2011-2019 Mellanox Technologies Ltd.
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

#include "resources.h"
#include "defs.h"

#include <stdio.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#else
#include <string.h>
#endif

#define TV_TO_USEC(tv) ((tv)->tv_sec * 1000000ULL + (tv)->tv_usec)
#define SAFE_SUB(x, y) ((y) < (x) ? (x) - (y) : 0)

static unsigned long long startup_timestamp;

void resources_init(void)
{
    int rc = 0;
#ifndef WIN32
    struct timeval tv;

    rc = gettimeofday(&tv, NULL);
    if (rc == 0)
        startup_timestamp = TV_TO_USEC(&tv);
#endif /* WIN32*/

    if (rc != 0)
        throw("resources_init failed");
}

void resources_get(resources_t *res)
{
    int rc = 0;
#ifndef WIN32
    struct rusage r;
    struct timeval tv;

#ifdef RUSAGE_THREAD
    /* RUSAGE_THREAD appeared in Linux 2.6.26, MacOS manual misses it too. */
    rc = getrusage(RUSAGE_THREAD, &r);
#else
    rc = getrusage(RUSAGE_SELF, &r);
#endif
    if (rc == 0) {
        res->utime = TV_TO_USEC(&r.ru_utime);
        res->stime = TV_TO_USEC(&r.ru_stime);
        res->nvcsw = r.ru_nvcsw;
        res->nivcsw = r.ru_nivcsw;
        rc = gettimeofday(&tv, NULL);
        if (rc == 0) {
            res->timestamp = TV_TO_USEC(&tv);
            res->rtime = SAFE_SUB(res->timestamp, startup_timestamp);
        }
    }
#else
    memset(res, 0, sizeof *res);
#endif /* WIN32 */

    if (rc != 0)
        throw("resources_get failed");
}

void resources_sub(resources_t *res1, resources_t *res2, resources_t *result)
{
    result->timestamp = res1->timestamp;
    result->rtime = SAFE_SUB(res1->timestamp, res2->timestamp);
    result->stime = SAFE_SUB(res1->stime, res2->stime);
    result->utime = SAFE_SUB(res1->utime, res2->utime);
    result->nvcsw = SAFE_SUB(res1->nvcsw, res2->nvcsw);
    result->nivcsw = SAFE_SUB(res1->nivcsw, res2->nivcsw);
}

void resources_print(resources_t *res)
{
#ifndef WIN32
    double cpu = 0.0;

    if (res->rtime != 0)
        cpu = (double)(res->stime + res->utime) / (double)res->rtime;
    log_msg("Resources usage: CPU=%.2f%%, nvcsw=%lu, nivcsw=%lu",
            cpu * 100.0, res->nvcsw, res->nivcsw);
#endif /* WIN32 */
}
