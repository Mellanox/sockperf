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
#ifndef VMA_REDIRECT_H
#define VMA_REDIRECT_H

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/poll.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <resolv.h>
#include <sys/epoll.h>

// -------------------------------------------------------------------
//
/**
 * Expected order of operations:
 * ==============================
 * 1. N x vma_setenv()
 * 2. vma_log_set_cb_func();
 * 3. vma_set_func_pointers();
 *
 * all functions return 'true' on success; 'false' - otherwise
 *
 * NOTE: including this header will redirect all relevant API calls to pointers
 * to functions. Hence, you you are allowed to call any of the relevant APIs,
 * only after successfully completing vma_set_func_pointers()!
 *
 */

//------------------------------------------------------------------------------
// The  vma_setenv()  function adds the variable 'name' to the environment with
// the value 'value', only if 'name' does not already exist.  If 'name' does exist
// in the environment, then the value of 'name' is not changed.
static inline bool vma_setenv(const char *name, const char *value)
{
	return setenv(name, value, 0) == 0;
}

//
//------------------------------------------------------------------------------
// NOTE: int log_level is equivalent to VMA_TRACELEVEL (see VMA README.txt)
typedef void (*vma_log_cb_t)(int log_level, const char* str);
//
// NOTE: log_cb will run in context of vma; hence, it must not block the thread
bool vma_log_set_cb_func(vma_log_cb_t log_cb);

//
//------------------------------------------------------------------------------
// loads libvma.so only in case 'loadVma' is set.  In any case sets
// global pointer variables for ALL vma related function pointers.
bool vma_set_func_pointers(bool loadVma);

//------------------------------------------------------------------------------
// loads libvma according to the given 'loadVmaPath',  then set
// global pointer variables for ALL vma related function pointers.
bool vma_set_func_pointers(const char *LibVmaPath);


/**
 *-----------------------------------------------------------------------------
 *  typedef for variables that will hold the function-pointers
 *-----------------------------------------------------------------------------
 */

typedef int (*socket_fptr_t)      (int __domain, int __type, int __protocol);
typedef int (*close_fptr_t)       (int __fd);
typedef int (*shutdown_fptr_t)    (int __fd, int __how);

typedef int (*accept_fptr_t)      (int __fd, struct sockaddr *__addr, socklen_t *__addrlen);
typedef int (*bind_fptr_t)        (int __fd, const struct sockaddr *__addr, socklen_t __addrlen);
typedef int (*connect_fptr_t)     (int __fd, const struct sockaddr *__to, socklen_t __tolen);
typedef int (*listen_fptr_t)      (int __fd, int __backlog);

typedef int (*setsockopt_fptr_t)  (int __fd, int __level, int __optname, __const void *__optval, socklen_t __optlen);
typedef int (*getsockopt_fptr_t)  (int __fd, int __level, int __optname, void *__optval, socklen_t *__optlen);
typedef int (*fcntl_fptr_t)       (int __fd, int __cmd, ...);
typedef int (*ioctl_fptr_t)       (int __fd, int __request, ...);
typedef int (*getsockname_fptr_t) (int __fd, struct sockaddr *__name,socklen_t *__namelen);
typedef int (*getpeername_fptr_t) (int __fd, struct sockaddr *__name,socklen_t *__namelen);

typedef ssize_t (*read_fptr_t)    (int __fd, void *__buf, size_t __nbytes);
typedef ssize_t (*readv_fptr_t)   (int __fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*recv_fptr_t)    (int __fd, void *__buf, size_t __n, int __flags);
typedef ssize_t (*recvmsg_fptr_t) (int __fd, struct msghdr *__message, int __flags);
typedef ssize_t (*recvfrom_fptr_t)(int __fd, void *__restrict __buf, size_t __n, int __flags, struct sockaddr *__from, socklen_t *__fromlen);

typedef ssize_t (*write_fptr_t)   (int __fd, __const void *__buf, size_t __n);
typedef ssize_t (*writev_fptr_t)  (int __fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*send_fptr_t)    (int __fd, __const void *__buf, size_t __n, int __flags);
typedef ssize_t (*sendmsg_fptr_t) (int __fd, __const struct msghdr *__message, int __flags);
typedef ssize_t (*sendto_fptr_t)  (int __fd, __const void *__buf, size_t __n,int __flags, const struct sockaddr *__to, socklen_t __tolen);

typedef int (*select_fptr_t)      (int __nfds, fd_set *__readfds, fd_set *__writefds, fd_set *__exceptfds, struct timeval *__timeout);

typedef int (*poll_fptr_t)        (struct pollfd *__fds, nfds_t __nfds, int __timeout);
typedef int (*epoll_create_fptr_t)(int __size);
typedef int (*epoll_ctl_fptr_t)   (int __epfd, int __op, int __fd, struct epoll_event *__event);
typedef int (*epoll_wait_fptr_t)  (int __epfd, struct epoll_event *__events, int __maxevents, int __timeout);

typedef int (*socketpair_fptr_t)  (int __domain, int __type, int __protocol, int __sv[2]);
typedef int (*pipe_fptr_t)        (int __filedes[2]);
typedef int (*open_fptr_t)        (__const char *__file, int __oflag, ...);
typedef int (*creat_fptr_t)       (const char *__pathname, mode_t __mode);
typedef int (*dup_fptr_t)         (int fildes);
typedef int (*dup2_fptr_t)        (int fildes, int fildes2);

typedef int (*clone_fptr_t)       (int (*__fn)(void *), void *__child_stack, int __flags, void *__arg);
typedef pid_t (*fork_fptr_t)      (void);
typedef int (*daemon_fptr_t)      (int __nochdir, int __noclose);
typedef int (*sigaction_fptr_t)   (int signum, const struct sigaction *act, struct sigaction *oldact);

/**
 *-----------------------------------------------------------------------------
 *  variables to hold the function-pointers
 *-----------------------------------------------------------------------------
 */
extern socket_fptr_t        fn_socket;
extern close_fptr_t         fn_close;
extern shutdown_fptr_t      fn_shutdown;
extern accept_fptr_t        fn_accept;
extern bind_fptr_t          fn_bind;
extern connect_fptr_t       fn_connect;
extern listen_fptr_t        fn_listen;

extern setsockopt_fptr_t    fn_setsockopt;
extern getsockopt_fptr_t    fn_getsockopt;
extern fcntl_fptr_t         fn_fcntl;
extern ioctl_fptr_t         fn_ioctl;
extern getsockname_fptr_t   fn_getsockname;
extern getpeername_fptr_t   fn_getpeername;
extern read_fptr_t          fn_read;
extern readv_fptr_t         fn_readv;
extern recv_fptr_t          fn_recv;
extern recvmsg_fptr_t       fn_recvmsg;
extern recvfrom_fptr_t      fn_recvfrom;
extern write_fptr_t         fn_write;
extern writev_fptr_t        fn_writev;
extern send_fptr_t          fn_send;
extern sendmsg_fptr_t       fn_sendmsg;
extern sendto_fptr_t        fn_sendto;
extern select_fptr_t        fn_select;
extern poll_fptr_t          fn_poll;
extern epoll_create_fptr_t  fn_epoll_create;
extern epoll_ctl_fptr_t     fn_epoll_ctl;
extern epoll_wait_fptr_t    fn_epoll_wait;
extern socketpair_fptr_t    fn_socketpair;
extern pipe_fptr_t          fn_pipe;
extern open_fptr_t          fn_open;
extern creat_fptr_t         fn_creat;
extern dup_fptr_t           fn_dup;
extern dup2_fptr_t          fn_dup2;
extern clone_fptr_t         fn_clone;
extern fork_fptr_t          fn_fork;
extern daemon_fptr_t        fn_daemon;
extern sigaction_fptr_t     fn_sigaction;


#ifndef VMA_NO_FUNCTIONS_DEFINES
// The following definitions will replace ALL relevant API calls with calls to our function pointers.
// NOTE: before any relevant API call, you MUST set our function pointers thru successful call to 'vma_set_func_pointers'
// (note: these definitions will not catch function prototypes neither combinations like 'struct sigaction')
#define socket(...)       fn_socket (__VA_ARGS__)
#define close(...)        fn_close (__VA_ARGS__)
#define shutdown(...)     fn_shutdown (__VA_ARGS__)

#define accept(...)       fn_accept (__VA_ARGS__)
#define bind(...)         fn_bind (__VA_ARGS__)
#define connect(...)      fn_connect (__VA_ARGS__)
#define listen(...)       fn_listen (__VA_ARGS__)

#define setsockopt(...)   fn_setsockopt (__VA_ARGS__)
#define getsockopt(...)   fn_getsockopt (__VA_ARGS__)
#define fcntl(...)        fn_fcntl (__VA_ARGS__)
#define ioctl(...)        fn_ioctl (__VA_ARGS__)
#define getsockname(...)  fn_getsockname (__VA_ARGS__)
#define getpeername(...)  fn_getpeername (__VA_ARGS__)

#define read(...)         fn_read (__VA_ARGS__)
#define readv(...)        fn_readv (__VA_ARGS__)
#define recv(...)         fn_recv (__VA_ARGS__)
#define recvmsg(...)      fn_recvmsg (__VA_ARGS__)
#define recvfrom(...)     fn_recvfrom (__VA_ARGS__)

#define write(...)        fn_write (__VA_ARGS__)
#define writev(...)       fn_writev (__VA_ARGS__)
#define send(...)         fn_send (__VA_ARGS__)
#define sendmsg(...)      fn_sendmsg (__VA_ARGS__)
#define sendto(...)       fn_sendto (__VA_ARGS__)

#define select(...)       fn_select (__VA_ARGS__)
#define poll(...)         fn_poll (__VA_ARGS__)
#define epoll_create(...) fn_epoll_create (__VA_ARGS__)
#define epoll_ctl(...)    fn_epoll_ctl (__VA_ARGS__)
#define epoll_wait(...)   fn_epoll_wait (__VA_ARGS__)

#define socketpair(...)   fn_socketpair (__VA_ARGS__)
#define pipe(...)         fn_pipe (__VA_ARGS__)
#define open(...)         fn_open (__VA_ARGS__)
#define creat(...)        fn_creat (__VA_ARGS__)
#define dup(...)          fn_dup (__VA_ARGS__)
#define dup2(...)         fn_dup2 (__VA_ARGS__)

#define clone(...)        fn_clone (__VA_ARGS__)
#define fork(...)         fn_fork (__VA_ARGS__)
#define daemon(...)       fn_daemon (__VA_ARGS__)
#define sigaction(...)    fn_sigaction(__VA_ARGS__)

#endif // VMA_NO_FUNCTIONS_DEFINES


#endif  //VMA_REDIRECT_H
