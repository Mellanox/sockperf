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

#include "vma-redirect.h"
#include <dlfcn.h>


socket_fptr_t        fn_socket       = NULL;
close_fptr_t         fn_close        = NULL;
shutdown_fptr_t      fn_shutdown     = NULL;
accept_fptr_t        fn_accept       = NULL;
bind_fptr_t          fn_bind         = NULL;
connect_fptr_t       fn_connect      = NULL;
listen_fptr_t        fn_listen       = NULL;

setsockopt_fptr_t    fn_setsockopt   = NULL;
getsockopt_fptr_t    fn_getsockopt   = NULL;
fcntl_fptr_t         fn_fcntl        = NULL;
ioctl_fptr_t         fn_ioctl        = NULL;
getsockname_fptr_t   fn_getsockname  = NULL;
getpeername_fptr_t   fn_getpeername  = NULL;
read_fptr_t          fn_read         = NULL;
readv_fptr_t         fn_readv        = NULL;
recv_fptr_t          fn_recv         = NULL;
recvmsg_fptr_t       fn_recvmsg      = NULL;
recvfrom_fptr_t      fn_recvfrom     = NULL;
write_fptr_t         fn_write        = NULL;
writev_fptr_t        fn_writev       = NULL;
send_fptr_t          fn_send         = NULL;
sendmsg_fptr_t       fn_sendmsg      = NULL;
sendto_fptr_t        fn_sendto       = NULL;
select_fptr_t        fn_select       = NULL;
poll_fptr_t          fn_poll         = NULL;
epoll_create_fptr_t  fn_epoll_create = NULL;
epoll_ctl_fptr_t     fn_epoll_ctl    = NULL;
epoll_wait_fptr_t    fn_epoll_wait   = NULL;

socketpair_fptr_t    fn_socketpair   = NULL;
pipe_fptr_t          fn_pipe         = NULL;
open_fptr_t          fn_open         = NULL;
creat_fptr_t         fn_creat        = NULL;

dup_fptr_t           fn_dup          = NULL;
dup2_fptr_t          fn_dup2         = NULL;
clone_fptr_t         fn_clone        = NULL;
fork_fptr_t          fn_fork         = NULL;
daemon_fptr_t        fn_daemon       = NULL;
sigaction_fptr_t     fn_sigaction    = NULL;


////////////////////////////////////////////////////////////////////////////////
#define VMA_LOG_CB_ENV_VAR "VMA_LOG_CB_FUNC_PTR"

////////////////////////////////////////////////////////////////////////////////
static vma_log_cb_t vma_log_get_cb_func()
{
	vma_log_cb_t log_cb = NULL;
	const char* const CB_STR = getenv(VMA_LOG_CB_ENV_VAR);
	if (!CB_STR || !*CB_STR) return NULL;

	if (1 != sscanf(CB_STR, "%p", &log_cb)) return NULL;
	return log_cb;
}

////////////////////////////////////////////////////////////////////////////////
bool vma_log_set_cb_func(vma_log_cb_t log_cb)
{
	char str[64];
	sprintf(str, "%p", log_cb);
	setenv(VMA_LOG_CB_ENV_VAR, str, 1);

	if (log_cb != vma_log_get_cb_func()) // verify that VMA will be able to read it correctly
	{
		unsetenv(VMA_LOG_CB_ENV_VAR);
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
static bool vma_set_func_pointers_internal(void *libHandle)
{
	// Set pointers to functions
	#define SET_FUNC_POINTER(libHandle, func)   (fn_##func = (func##_fptr_t)dlsym(libHandle, #func))

	if (! SET_FUNC_POINTER(libHandle, socket))       return false;
	if (! SET_FUNC_POINTER(libHandle, close))        return false;
	if (! SET_FUNC_POINTER(libHandle, shutdown))     return false;

	if (! SET_FUNC_POINTER(libHandle, accept))       return false;
	if (! SET_FUNC_POINTER(libHandle, bind))         return false;
	if (! SET_FUNC_POINTER(libHandle, connect))      return false;
	if (! SET_FUNC_POINTER(libHandle, listen))      return false;

	if (! SET_FUNC_POINTER(libHandle, setsockopt))   return false;
	if (! SET_FUNC_POINTER(libHandle, getsockopt))   return false;
	if (! SET_FUNC_POINTER(libHandle, fcntl))        return false;
	if (! SET_FUNC_POINTER(libHandle, ioctl))        return false;
	if (! SET_FUNC_POINTER(libHandle, getsockname))  return false;
	if (! SET_FUNC_POINTER(libHandle, getpeername))  return false;

	if (! SET_FUNC_POINTER(libHandle, read))         return false;
	if (! SET_FUNC_POINTER(libHandle, readv))        return false;
	if (! SET_FUNC_POINTER(libHandle, recv))         return false;
	if (! SET_FUNC_POINTER(libHandle, recvmsg))      return false;
	if (! SET_FUNC_POINTER(libHandle, recvfrom))     return false;

	if (! SET_FUNC_POINTER(libHandle, write))        return false;
	if (! SET_FUNC_POINTER(libHandle, writev))       return false;
	if (! SET_FUNC_POINTER(libHandle, send))         return false;
	if (! SET_FUNC_POINTER(libHandle, sendmsg))      return false;
	if (! SET_FUNC_POINTER(libHandle, sendto))       return false;

	if (! SET_FUNC_POINTER(libHandle, select))       return false;
	if (! SET_FUNC_POINTER(libHandle, poll))         return false;
	if (! SET_FUNC_POINTER(libHandle, epoll_create)) return false;
	if (! SET_FUNC_POINTER(libHandle, epoll_ctl))    return false;
	if (! SET_FUNC_POINTER(libHandle, epoll_wait))   return false;

	if (! SET_FUNC_POINTER(libHandle, socketpair))   return false;
	if (! SET_FUNC_POINTER(libHandle, pipe))         return false;
	if (! SET_FUNC_POINTER(libHandle, open))         return false;
	if (! SET_FUNC_POINTER(libHandle, creat))        return false;
	if (! SET_FUNC_POINTER(libHandle, dup))          return false;
	if (! SET_FUNC_POINTER(libHandle, dup2))         return false;

	if (! SET_FUNC_POINTER(libHandle, clone))        return false;
	if (! SET_FUNC_POINTER(libHandle, fork))         return false;
	if (! SET_FUNC_POINTER(libHandle, daemon))       return false;
	if (! SET_FUNC_POINTER(libHandle, sigaction))    return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool vma_set_func_pointers(bool loadVma)
{
	void * libHandle = RTLD_DEFAULT;
	if (loadVma) {
		const char* libName = "libvma.so";
//		libHandle = dlopen(libName, RTLD_NOW);  // this was broken in vma_tcp_4.5 because of symbol: vma_log_set_log_stderr
		libHandle = dlopen(libName, RTLD_LAZY);
		if (! libHandle) return false;
	}
	return vma_set_func_pointers_internal(libHandle);
}


//------------------------------------------------------------------------------
bool vma_set_func_pointers(const char *LibVmaPath)
{
	if (!LibVmaPath || !* LibVmaPath) return false;

	//void * libHandle = dlopen(LibVmaPath, RTLD_NOW);  // this was broken in vma_tcp_4.5 because of symbol: vma_log_set_log_stderr
	void * libHandle = dlopen(LibVmaPath, RTLD_LAZY);

	if (! libHandle) return false;

	return vma_set_func_pointers_internal(libHandle);
}

