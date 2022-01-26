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

/**
 * @file resources.h
 *
 * @details: Resources usage statistics
 *
 * @author  Dmytro Podgornyi <dmytrop@mellanox.com>
 *  reviewed by
 *
 **/

#ifndef _RESOURCES_H_
#define _RESOURCES_H_

typedef struct resources_t {
    /** Timestamp, epoch time */
    unsigned long long timestamp;
    /** Run time */
    unsigned long long rtime;
    /** System time (how long CPU spent in kernelspace) */
    unsigned long long stime;
    /** User time (how long CPU spent in userspace) */
    unsigned long long utime;
    /** Voluntary context switches */
    unsigned long nvcsw;
    /** Involuntary context switches */
    unsigned long nivcsw;
} resources_t;

/**
 * Initializes resources subsystem. Must be called at startup.
 */
void resources_init(void);

/**
 * Stores counters since the start of the process.
 */
void resources_get(resources_t *res);

/**
 * result = res1 - res2.
 * Finds how much resources are used within the period between res2 and res1.
 */
void resources_sub(resources_t *res1, resources_t *res2, resources_t *result);

/**
 * Logs resources usage in human readable format. res can be produced by both
 * resources_get() and resources_sub().
 */
void resources_print(resources_t *res);

#endif /* _RESOURCES_H_ */
