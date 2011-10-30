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
	printf("sockperf: program exits because of an error.  current value of errno=%d (%m)\n", errno);
#ifdef DEBUG
	printf_backtrace();
#endif
	exit(status);
}

//------------------------------------------------------------------------------
void exit_with_log(const char* error, int status)
{
	log_err("%s",error);
	cleanup();
	#ifdef DEBUG
		printf_backtrace();
	#endif
	exit(status);
}

//------------------------------------------------------------------------------
void exit_with_log(const char* error, int status, fds_data* fds)
{
	printf("IP = %-15s PORT = %5d # %s ",
			inet_ntoa(fds->addr.sin_addr),
			ntohs(fds->addr.sin_port),
			PRINT_PROTOCOL(fds->sock_type));
	exit_with_log(error, status);
}
void exit_with_err(const char* error, int status)
{
	log_err("%s",error);
	#ifdef DEBUG
		printf_backtrace();
	#endif
	exit(status);
}
void print_log_dbg ( struct in_addr sin_addr,in_port_t sin_port, int ifd)
{
	log_dbg ("peer address to close: %s:%d [%d]",inet_ntoa(sin_addr), ntohs(sin_port), ifd);
}
//------------------------------------------------------------------------------
int set_affinity_list(pthread_t tid, const char * cpu_list)
{
	int rc = SOCKPERF_ERR_NONE;

	if (cpu_list && cpu_list[0]) {
        long cpu_from = -1;
        long cpu_cur = 0;
    	char * buf = strdup(cpu_list);
    	char * cur_buf = buf;
    	char * cur_ptr = buf;
    	char * end_ptr = NULL;
    	cpu_set_t cpuset;

		CPU_ZERO(&cpuset);

		/* Parse cpu list */
		while (cur_buf) {
			if (*cur_ptr == '\0') {
				cpu_cur = strtol(cur_buf, &end_ptr, 0);
				if ( (errno != 0) || (cur_buf == end_ptr) ) {
					log_err("Invalid argument: %s", cpu_list);
					rc = SOCKPERF_ERR_BAD_ARGUMENT;
					break;
				}
				cur_buf = NULL;
			}
			else if (*cur_ptr == ',') {
				*cur_ptr = '\0';
				cpu_cur = strtol(cur_buf, &end_ptr, 0);
				if ( (errno != 0) || (cur_buf == end_ptr) ) {
					log_err("Invalid argument: %s", cpu_list);
					rc = SOCKPERF_ERR_BAD_ARGUMENT;
					break;
				}
				cur_buf = cur_ptr + 1;
				cur_ptr++;
			}
			else if (*cur_ptr == '-') {
				*cur_ptr = '\0';
				cpu_from = strtol(cur_buf, &end_ptr, 0);
				if ( (errno != 0) || (cur_buf == end_ptr) ) {
					log_err("Invalid argument: %s", cpu_list);
					rc = SOCKPERF_ERR_BAD_ARGUMENT;
					break;
				}
				cur_buf = cur_ptr + 1;
				cur_ptr++;
				continue;
			}
			else {
				cur_ptr++;
				continue;
			}

			if ((cpu_from <= cpu_cur) && (cpu_cur < CPU_SETSIZE)) {
				if (cpu_from == -1) cpu_from = cpu_cur;

				while ((cpu_from <= cpu_cur)) {
					CPU_SET(cpu_from, &cpuset);
					cpu_from++;
				}
				cpu_from = -1;
			}
			else {
				log_err("Invalid argument: %s", cpu_list);
				rc = SOCKPERF_ERR_BAD_ARGUMENT;
				break;
			}
		}

		if ( (rc == SOCKPERF_ERR_NONE) &&
			 (0 != pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset)) )
		{
			log_err("pthread_setaffinity_np failed to set tid(%lu) to cpu(%s)", tid, cpu_list);
			rc = SOCKPERF_ERR_FATAL;
		}

		if (buf) {
			free(buf);
		}
	}

	return rc;
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

//------------------------------------------------------------------------------
const char* handler2str( fd_block_handler_t type )
{
	static const char* s_fds_handle_desc[FD_HANDLE_MAX] =
	{
		"recvfrom",
		"select",
		"poll",
		"epoll"
	};

	return s_fds_handle_desc[type];
}
