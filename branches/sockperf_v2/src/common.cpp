 /*
 * Copyright (c) 2011 Mellanox Technologies Ltd.
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
 *
 */
#include "common.h"
#include <execinfo.h>  // for backtrace

extern void cleanup();

//
// AvnerB: the purpose of these error functions is to take out error code from inline function
//
//------------------------------------------------------------------------------
void recvfromError(int fd)
{
	if (!g_b_exit) {
		log_err("recvfrom() Failed receiving on fd[%d]", fd);
		exit_with_log(SOCKPERF_ERR_SOCKET);
	}
}

//------------------------------------------------------------------------------
void sendtoError(int fd, int nbytes, const struct sockaddr_in *sendto_addr) {
	if (!g_b_exit) {
		log_err("sendto() Failed sending on fd[%d] to %s:%d msg size of %d bytes", fd, inet_ntoa(sendto_addr->sin_addr), ntohs(sendto_addr->sin_port), nbytes);
		exit_with_log(SOCKPERF_ERR_SOCKET);
	}
}

//------------------------------------------------------------------------------
pid_t gettid(void)
{
	return syscall(__NR_gettid);
}

//------------------------------------------------------------------------------
void printf_backtrace(void)
{
	char **strings;
	void* m_backtrace[25];
	int m_backtrace_size = backtrace(m_backtrace, 25);
	printf("sockperf: [tid: %d] ------\n", gettid());
	strings = backtrace_symbols(m_backtrace, m_backtrace_size);
	for (int i = 0; i < m_backtrace_size; i++)
		printf("sockperf: [%i] %p: %s\n", i, m_backtrace[i], strings[i]);
	free(strings);
}

//------------------------------------------------------------------------------
void exit_with_log(int status)
{
	cleanup();
	printf("sockperf: program exits because of an error.  current value of errno=%d (%m)", errno);
	printf_backtrace();
	exit(status);
}

//------------------------------------------------------------------------------
int set_affinity(pthread_t tid, int cpu)
{
	int rc = SOCKPERF_ERR_NONE;

	if (cpu != -1) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		if (0 != pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset))
		{
			log_err("pthread_setaffinity_np failed to set tid(%lu) to cpu(%d)", tid, cpu);
			rc = SOCKPERF_ERR_FATAL;
		}
	}

	return rc;
}

//------------------------------------------------------------------------------
void hexdump(void *ptr, int buflen)
{
    unsigned char *buf = (unsigned char*)ptr;
    int i, j;

    for (i=0; i<buflen; i+=16)
    {
        printf("%06x: ", i);
        for (j=0; j<16; j++)
        if (i+j < buflen)
            printf("%02x ", buf[i+j]);
        else
            printf("   ");
        printf(" ");
        for (j=0; j<16; j++)
            if (i+j < buflen)
                printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
        printf("\n");
    }
}
