/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef PORT_DESCRIPTOR_H_
#define PORT_DESCRIPTOR_H_

#include <stdint.h>

#ifdef __windows__

typedef uint16_t in_port_t;

#else

#include <netinet/in.h>  /* internet address manipulation */

#endif // __windows__

typedef struct port_descriptor {
    int sock_type; /**< SOCK_STREAM (tcp), SOCK_DGRAM (udp), SOCK_RAW (ip) */
    sa_family_t family; // AF_INET, AF_INET6
    in_port_t port;
} port_type;

// The only types that TR1 has built-in hash/equal_to functions for, are scalar types,
// std:: string, and std::wstring. For any other type, we need to write a
// hash/equal_to functions, by ourself.
namespace std {

template <>
struct hash<struct port_descriptor> : public std::unary_function<struct port_descriptor, int> {
    int operator()(struct port_descriptor const &key) const {
        return key.sock_type ^ key.port ^ key.family;
    }
};

template <>
struct equal_to<struct port_descriptor> : public std::binary_function<struct port_descriptor,
                                                                    struct port_descriptor, bool> {
    bool operator()(struct port_descriptor const &key1, struct port_descriptor const &key2) const {
        return key1.sock_type == key2.sock_type
            && key1.family == key2.family
            && key1.port == key2.port;
    }
};

} // namespace std

#endif // PORT_DESCRIPTOR_H_
