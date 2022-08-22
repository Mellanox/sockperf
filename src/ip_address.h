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

#ifndef IP_ADDRESS_H_
#define IP_ADDRESS_H_

#ifdef __windows__
#include <WS2tcpip.h>
#include <Winsock2.h>
#include <unordered_map>
#include <Winbase.h>
#include <stdint.h>
#include <afunix.h>
typedef unsigned short int sa_family_t;

#elif __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>      /* definitions for UNIX domain sockets */

#endif

#include "os_abstract.h"
#include <functional>
#include <string>

#include <unordered_map>

class IPAddress {
private:
    sa_family_t m_family;
    union {
        in_addr m_addr4;
        in6_addr m_addr6;
    };
    std::string m_addr_un;

public:
    static bool resolve(const char *str, IPAddress &out, std::string &err);
    static IPAddress zero();

    IPAddress(const IPAddress &rhs);
    IPAddress(const sockaddr *sa, socklen_t len);
    IPAddress() : m_family(AF_UNSPEC)
    {
    }

    inline sa_family_t family() const
    {
        return m_family;
    }

    inline const in_addr &addr4() const
    {
        return m_addr4;
    }

    inline const in6_addr &addr6() const
    {
        return m_addr6;
    }

    inline std::string addr_un() const
    {
        return m_addr_un;
    }

    bool is_specified() const;

    std::string toString() const;

    inline friend bool operator==(const IPAddress &key1, const IPAddress &key2)
    {
        if (key1.m_family == key2.m_family) {
            switch (key1.m_family) {
            case AF_UNSPEC:
                return true;
            case AF_INET:
                return key1.m_addr4.s_addr == key2.m_addr4.s_addr;
            case AF_INET6:
                return IN6_ARE_ADDR_EQUAL(&key1.m_addr6, &key2.m_addr6);
            case AF_UNIX:
                return key1.m_addr_un.compare(key2.m_addr_un) == 0;
            }
        }
        return false;
    }
};

// The only types that TR1 has built-in hash/equal_to functions for, are scalar types,
// std:: string, and std::wstring. For any other type, we need to write a
// hash/equal_to functions, by ourself.
namespace std {
template <> struct hash<IPAddress> : public std::unary_function<IPAddress, int> {
    int operator()(IPAddress const &key) const
    {
        switch (key.family()) {
        case AF_INET:
            return key.addr4().s_addr & 0xFF;
        case AF_INET6: {
            const uint32_t *addr = reinterpret_cast<const uint32_t *>(&key.addr6());
            return addr[0] ^ addr[1] ^ addr[2] ^ addr[3];
        }
        default:
            return 0;
        }
    }
};

template <>
struct equal_to<IPAddress> : public std::binary_function<IPAddress, IPAddress,
                                                              bool> {
    bool operator()(IPAddress const &key1, IPAddress const &key2) const {
        return key1 == key2;
    }
};

} // namespace std

#endif // IP_ADDRESS_H_
