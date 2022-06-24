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

#include "vma-xlio-redirect.h"
#include <dlfcn.h>

socket_fptr_t fn_socket = NULL;
close_fptr_t fn_close = NULL;
shutdown_fptr_t fn_shutdown = NULL;
accept_fptr_t fn_accept = NULL;
bind_fptr_t fn_bind = NULL;
connect_fptr_t fn_connect = NULL;
listen_fptr_t fn_listen = NULL;

setsockopt_fptr_t fn_setsockopt = NULL;
getsockopt_fptr_t fn_getsockopt = NULL;
fcntl_fptr_t fn_fcntl = NULL;
ioctl_fptr_t fn_ioctl = NULL;
getsockname_fptr_t fn_getsockname = NULL;
getpeername_fptr_t fn_getpeername = NULL;
read_fptr_t fn_read = NULL;
readv_fptr_t fn_readv = NULL;
recv_fptr_t fn_recv = NULL;
recvmsg_fptr_t fn_recvmsg = NULL;
recvmmsg_fptr_t fn_recvmmsg = NULL;
recvfrom_fptr_t fn_recvfrom = NULL;
write_fptr_t fn_write = NULL;
writev_fptr_t fn_writev = NULL;
send_fptr_t fn_send = NULL;
sendmsg_fptr_t fn_sendmsg = NULL;
sendmmsg_fptr_t fn_sendmmsg = NULL;
sendto_fptr_t fn_sendto = NULL;
select_fptr_t fn_select = NULL;
pselect_fptr_t fn_pselect = NULL;
poll_fptr_t fn_poll = NULL;
ppoll_fptr_t fn_ppoll = NULL;
epoll_create_fptr_t fn_epoll_create = NULL;
epoll_create1_fptr_t fn_epoll_create1 = NULL;
epoll_ctl_fptr_t fn_epoll_ctl = NULL;
epoll_wait_fptr_t fn_epoll_wait = NULL;
epoll_pwait_fptr_t fn_epoll_pwait = NULL;

socketpair_fptr_t fn_socketpair = NULL;
pipe_fptr_t fn_pipe = NULL;
open_fptr_t fn_open = NULL;
creat_fptr_t fn_creat = NULL;

dup_fptr_t fn_dup = NULL;
dup2_fptr_t fn_dup2 = NULL;
clone_fptr_t fn_clone = NULL;
fork_fptr_t fn_fork = NULL;
vfork_fptr_t fn_vfork = NULL;
daemon_fptr_t fn_daemon = NULL;
sigaction_fptr_t fn_sigaction = NULL;

////////////////////////////////////////////////////////////////////////////////
#define VMA_LOG_CB_ENV_VAR  "VMA_LOG_CB_FUNC_PTR"
#define XLIO_LOG_CB_ENV_VAR "XLIO_LOG_CB_FUNC_PTR"

////////////////////////////////////////////////////////////////////////////////
static vma_xlio_log_cb_t vma_xlio_log_get_cb_func() {
    vma_xlio_log_cb_t log_cb = NULL;
    const char *const CB_STR = getenv(VMA_LOG_CB_ENV_VAR);
    if (!CB_STR || !*CB_STR) return NULL;

    if (1 != sscanf(CB_STR, "%p", &log_cb)) return NULL;
    return log_cb;
}

////////////////////////////////////////////////////////////////////////////////
bool vma_xlio_log_set_cb_func(vma_xlio_log_cb_t log_cb) {
    char str[64];
    sprintf(str, "%p", log_cb);
    setenv(VMA_LOG_CB_ENV_VAR, str, 1);
    setenv(XLIO_LOG_CB_ENV_VAR, str, 1);

    // verify that VMA will be able to read it correctly. it's enough to check
    // only for VMA case since the code are identical
    if (log_cb != vma_xlio_log_get_cb_func())
    {
        unsetenv(VMA_LOG_CB_ENV_VAR);
        unsetenv(XLIO_LOG_CB_ENV_VAR);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
static bool vma_xlio_set_func_pointers_internal(void *libHandle) {
// Set pointers to functions
#define SET_FUNC_POINTER(libHandle, func) (fn_##func = (func##_fptr_t)dlsym(libHandle, #func))

    bool ret = true;

    if (!SET_FUNC_POINTER(libHandle, socket)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, close)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, shutdown)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, accept)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, bind)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, connect)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, listen)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, setsockopt)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, getsockopt)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, fcntl)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, ioctl)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, getsockname)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, getpeername)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, read)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, readv)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, recv)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, recvmsg)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, recvmmsg)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, recvfrom)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, write)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, writev)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, send)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, sendmsg)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, sendmmsg)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, sendto)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, select)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, pselect)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, poll)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, ppoll)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, epoll_create)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, epoll_create1)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, epoll_ctl)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, epoll_wait)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, epoll_pwait)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, socketpair)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, pipe)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, open)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, creat)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, dup)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, dup2)) ret = false;

    if (!SET_FUNC_POINTER(libHandle, clone)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, fork)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, vfork)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, daemon)) ret = false;
    if (!SET_FUNC_POINTER(libHandle, sigaction)) ret = false;

    return ret;
}

////////////////////////////////////////////////////////////////////////////////
bool vma_xlio_try_set_func_pointers() {
    void *libHandle = RTLD_DEFAULT;
    return vma_xlio_set_func_pointers_internal(libHandle);
}

//------------------------------------------------------------------------------
bool vma_xlio_set_func_pointers(const char *loadLibPath) {
    if (!loadLibPath || !*loadLibPath) return false;

    // void * libHandle = dlopen(loadLibPath, RTLD_NOW);  // this was broken in vma_tcp_4.5 because
    // of symbol: vma_log_set_log_stderr
    void *libHandle = dlopen(loadLibPath, RTLD_LAZY);

    if (!libHandle) return false;

    return vma_xlio_set_func_pointers_internal(libHandle);
}
