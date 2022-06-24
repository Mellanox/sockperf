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

#include "ip_address.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

bool IPAddress::resolve(const char *str, IPAddress &out, std::string &err)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    // allow IPv4 or IPv6
    hints.ai_family = AF_UNSPEC;
    // return addresses only from configured address families
    hints.ai_flags = AI_ADDRCONFIG;
    // any protocol
    hints.ai_protocol = 0;
    // any socket type
    hints.ai_socktype = 0;

    struct addrinfo *result;
    int res = getaddrinfo(str, NULL, &hints, &result);
    if (res == 0) {
        out.m_family = AF_UNSPEC;
        for (const addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
            if (rp->ai_family == AF_INET6) {
                out.m_family = rp->ai_family;
                out.m_addr6 = reinterpret_cast<const sockaddr_in6 *>(rp->ai_addr)->sin6_addr;
            }
        }
        if (out.m_family == AF_UNSPEC) {
            for (addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
                out.m_family = rp->ai_family;
                out.m_addr4 = reinterpret_cast<const sockaddr_in *>(rp->ai_addr)->sin_addr;
            }
        }
    } else {
        err = gai_strerror(res);
    }
    freeaddrinfo(result);

    return res == 0;
}

IPAddress IPAddress::zero()
{
    IPAddress res;
    memset(&res.m_addr6, 0, sizeof(res.m_addr6));
    return res;
}

bool IPAddress::is_specified() const
{
    switch (m_family) {
    case AF_INET:
        return m_addr4.s_addr != INADDR_ANY;
    case AF_INET6:
        return !IN6_IS_ADDR_UNSPECIFIED(&m_addr6);
    }
    return false;
}

std::string IPAddress::toString() const
{
    if (m_family == AF_UNSPEC) {
        return "(unspec)";
    }
    char hbuf[INET6_ADDRSTRLEN];
    const char *res = inet_ntop(m_family, &m_addr4, hbuf, INET6_ADDRSTRLEN);
    if (res) {
        return res;
    } else {
        return "(unknown)";
    }
}
