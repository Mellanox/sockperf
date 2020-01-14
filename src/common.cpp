/*
 * Copyright (c) 2011-2020 Mellanox Technologies Ltd.
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

#include "common.h"

extern void cleanup();

user_params_t s_user_params;
//
// AvnerB: the purpose of these error functions is to take out error code from inline function
//
//------------------------------------------------------------------------------
void recvfromError(int fd) {
    if (!g_b_exit) {
        log_err("recvfrom() Failed receiving on fd[%d]", fd);
        exit_with_log(SOCKPERF_ERR_SOCKET);
    }
}

//------------------------------------------------------------------------------
void sendtoError(int fd, int nbytes, const struct sockaddr_in *sendto_addr) {
    if (!g_b_exit) {
        log_err("sendto() Failed sending on fd[%d] to %s:%d msg size of %d bytes", fd,
                inet_ntoa(sendto_addr->sin_addr), ntohs(sendto_addr->sin_port), nbytes);
        exit_with_log(SOCKPERF_ERR_SOCKET);
    }
}

//------------------------------------------------------------------------------
void exit_with_log(int status) { exit_with_log("program exits because of an error.", status); }

//------------------------------------------------------------------------------
void exit_with_log(const char *error, int status) {
    log_err("%s", error);
    g_b_errorOccured = true;
    g_b_exit = true;
    cleanup();
#ifdef DEBUG
    os_printf_backtrace();
#endif
    exit(status);
}

//------------------------------------------------------------------------------
void exit_with_log(const char *error, int status, fds_data *fds) {
    printf("IP = %-15s PORT = %5d # %s ", inet_ntoa(fds->server_addr.sin_addr),
           ntohs(fds->server_addr.sin_port), PRINT_PROTOCOL(fds->sock_type));
    exit_with_log(error, status);
}

//------------------------------------------------------------------------------
void exit_with_err(const char *error, int status) {
    log_err("%s", error);
#ifdef DEBUG
    os_printf_backtrace();
#endif
    exit(status);
}

//------------------------------------------------------------------------------
void print_log_dbg(struct in_addr sin_addr, in_port_t sin_port, int ifd) {
    log_dbg("peer address to close: %s:%d [%d]", inet_ntoa(sin_addr), ntohs(sin_port), ifd);
}

//------------------------------------------------------------------------------
int set_affinity_list(os_thread_t thread, const char *cpu_list) {
    int rc = SOCKPERF_ERR_NONE;

    if (cpu_list && cpu_list[0]) {
        long cpu_from = -1;
        long cpu_cur = 0;
        char *buf = strdup(cpu_list);
        char *cur_buf = buf;
        char *cur_ptr = buf;
        char *end_ptr = NULL;

        os_cpuset_t mycpuset;
        os_init_cpuset(&mycpuset);

        /* Parse cpu list */
        while (cur_buf) {
            errno = 0;
            if (*cur_ptr == '\0') {
                cpu_cur = strtol(cur_buf, &end_ptr, 0);
                if ((errno != 0) || (cur_buf == end_ptr)) {
                    log_err("Invalid argument: %s", cpu_list);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    break;
                }
                cur_buf = NULL;
            } else if (*cur_ptr == ',') {
                *cur_ptr = '\0';
                cpu_cur = strtol(cur_buf, &end_ptr, 0);
                if ((errno != 0) || (cur_buf == end_ptr)) {
                    log_err("Invalid argument: %s", cpu_list);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    break;
                }
                cur_buf = cur_ptr + 1;
                cur_ptr++;
            } else if (*cur_ptr == '-') {
                *cur_ptr = '\0';
                cpu_from = strtol(cur_buf, &end_ptr, 0);
                if ((errno != 0) || (cur_buf == end_ptr)) {
                    log_err("Invalid argument: %s", cpu_list);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    break;
                }
                cur_buf = cur_ptr + 1;
                cur_ptr++;
                continue;
            } else {
                cur_ptr++;
                continue;
            }

            if ((cpu_from <= cpu_cur) && (cpu_cur < CPU_SETSIZE)) {
                if (cpu_from == -1) cpu_from = cpu_cur;

                os_cpu_set(&mycpuset, cpu_from, cpu_cur);

                cpu_from = -1;
            } else {
                log_err("Invalid argument: %s", cpu_list);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
                break;
            }
        }
        if ((rc == SOCKPERF_ERR_NONE) && os_set_affinity(thread, mycpuset)) {
            log_err("Set thread affinity failed to set tid(%lu) to cpu(%s)",
                    (unsigned long)thread.tid, cpu_list);
            rc = SOCKPERF_ERR_FATAL;
        }

        if (buf) {
            free(buf);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void hexdump(void *ptr, int buflen) {
    unsigned char *buf = (unsigned char *)ptr;
    int i, j;

    for (i = 0; i < buflen; i += 16) {
        printf("%06x: ", i);
        for (j = 0; j < 16; j++)
            if (i + j < buflen)
                printf("%02x ", buf[i + j]);
            else
                printf("   ");
        printf(" ");
        for (j = 0; j < 16; j++)
            if (i + j < buflen) printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        printf("\n");
    }
}

//------------------------------------------------------------------------------
const char *handler2str(fd_block_handler_t type) {
    // must be coordinated with fd_block_handler_t in defs.h
    static const char *s_fds_handle_desc[FD_HANDLE_MAX] = { "recvfrom", "recvfrom", "select"
#ifndef WIN32
                                                            ,
                                                            "poll", "epoll",
#ifdef USING_VMA_EXTRA_API
                                                            "socketxtreme"
#endif
#endif
    };

    return s_fds_handle_desc[type];
}

// Receives as input a path to a system file and returns the value stored in the file
int read_int_from_sys_file(const char *path) {
    int retVal;
    FILE *file = fopen(path, "r");

    if (!file) return -1;

    if (1 != fscanf(file, "%d", &retVal)) retVal = 0;
    fclose(file);

    return retVal;
}

int sock_set_rate_limit(int fd, uint32_t rate_limit) {
    int rc = SOCKPERF_ERR_NONE;
    if (setsockopt(fd, SOL_SOCKET, SO_MAX_PACING_RATE, &rate_limit, sizeof(rate_limit)) < 0) {
        log_err("setsockopt(SO_MAX_PACING_RATE), set rate-limit failed. "
                "It could be that this option is not supported in your system");
        rc = SOCKPERF_ERR_SOCKET;
    } else {
        log_msg("succeed to set sock-SO_MAX_PACING_RATE");
    }
    return rc;
}
