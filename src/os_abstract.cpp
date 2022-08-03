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

#include <stdio.h>
#include "os_abstract.h"
#include <string.h>

void os_printf_backtrace(void) {
#ifdef __windows__
    unsigned int i;
    void *stack[100];
    unsigned short frames;
    SYMBOL_INFO *symbol;
    HANDLE process;

    process = GetCurrentProcess();

    SymInitialize(process, NULL, TRUE);

    frames = CaptureStackBackTrace(0, 100, stack, NULL);
    symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);

        printf("%i: %s - 0x%llu\n", frames - i - 1, symbol->Name, symbol->Address);
    }

    free(symbol);
#else
    char **strings;
    void *m_backtrace[25];
    int m_backtrace_size = backtrace(m_backtrace, 25);
    printf("sockperf: [tid: %lu] ------\n", (unsigned long)os_getthread().tid);
    strings = backtrace_symbols(m_backtrace, m_backtrace_size);
    for (int i = 0; i < m_backtrace_size; i++)
        printf("sockperf: [%i] %p: %s\n", i, m_backtrace[i], strings[i]);
    free(strings);
#endif
}

// Thread functions

void os_thread_init(os_thread_t *thr) {
#ifdef __windows__
    thr->hThread = NULL;
    thr->tid = 0;
#else
    thr->tid = 0;
#endif
}

void os_thread_close(os_thread_t *thr) {
#ifdef __windows__
    if (thr->hThread) CloseHandle(thr->hThread);
#endif
}

void os_thread_detach(os_thread_t *thr) {
#ifndef __windows__
    pthread_detach(thr->tid);
#endif
}

int os_thread_exec(os_thread_t *thr, void *(*start)(void *), void *arg) {
#ifdef __windows__
    if (thr->hThread) CloseHandle(thr->hThread);
    thr->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start, arg, 0, &thr->tid);
    if (thr->hThread == INVALID_HANDLE_VALUE) {
        return -1;
    }
    return 0;
#else
    pthread_attr_t atts;
    pthread_attr_init(&atts);
#ifdef HAVE_PTHREAD_ATTR_SETSCOPE
    pthread_attr_setscope(&atts, PTHREAD_SCOPE_SYSTEM);
#endif
#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
    pthread_attr_setstacksize(&atts, 256 * 1024);
#endif
    return pthread_create(&thr->tid, &atts, start, arg);
#endif
}

void os_thread_kill(os_thread_t *thr) {
#ifdef __windows__
    DWORD exit_code;
    if (GetExitCodeThread(thr, &exit_code)) TerminateThread(thr, exit_code); // kill thread
#else
    pthread_kill(thr->tid, SIGINT);
#endif
}

void os_thread_join(os_thread_t *thr) {
#ifdef __windows__
    WaitForSingleObject(thr, INFINITE);
#else
    pthread_join(thr->tid, 0);
#endif
}

os_thread_t os_getthread(void) {
    os_thread_t mythread;
#ifdef __windows__
    mythread.tid = GetCurrentThreadId();
    mythread.hThread = GetCurrentThread();
#elif __FreeBSD__
    mythread.tid = pthread_self();
#else
    mythread.tid = syscall(__NR_gettid);
#endif
    return mythread;
}

// Mutex functions

void os_mutex_init(os_mutex_t *lock) {
#ifdef __windows__
    lock->mutex = CreateMutex(NULL, FALSE, NULL);
#else
    pthread_mutex_init(&lock->mutex, NULL);
#endif
}

void os_mutex_close(os_mutex_t *lock) {
#ifdef __windows__
    CloseHandle(lock->mutex);
#else
    pthread_mutex_destroy(&lock->mutex);
#endif
}

void os_mutex_lock(os_mutex_t *lock) {
#ifdef __windows__
    WaitForSingleObject(lock->mutex, INFINITE);
#else
    pthread_mutex_lock(&lock->mutex);
#endif
}

void os_mutex_unlock(os_mutex_t *lock) {
#ifdef __windows__
    ReleaseMutex(lock->mutex);
#else
    pthread_mutex_unlock(&lock->mutex);
#endif
}

int os_set_nonblocking_socket(int fd) {
#ifdef __windows__
    int iResult;
    u_long iMode = 1; // = Non-blocking
    iResult = ioctlsocket(fd, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        printf("ERROR: ioctlsocket failed with error: %ld\n", iResult);
        return -1;
    }
    return 0;
#else
    int flags, ret;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        printf("ERROR: fcntl(F_GETFL)\n");
        return -1;
    }
    flags |= O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flags);
    if (ret < 0) {
        printf("ERROR: fcntl(F_SETFL)\n");
        return -1;
    }
    return 0;
#endif
}

void os_set_signal_action(int signum, sig_handler handler) {
#ifdef __windows__
    // Meny: No SIGALRM on Windows, A different thread handles test duration timer.
    signal(signum, handler);
#else
    static struct sigaction sigact; // Avner: is 'static' needed?

    sigact.sa_handler = handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigaction(signum, &sigact, NULL);
#endif
}

int os_daemonize() {
#ifdef __windows__
    printf("ERROR: daemonize is not supported!\n");
    return -1;
#else
    if (daemon(1, 1)) {
        return -1;
    } else {
        return 0;
    }
#endif
}

#ifdef __windows__
struct TimeHandler {
    itimerval myTimer;
    sig_handler *handler;

    TimeHandler(itimerval _Timer, sig_handler _handler) {
        this->myTimer = _Timer;
        this->handler = _handler;
    }
};
#endif

int os_set_duration_timer(const itimerval &timer, sig_handler handler) {
    int ret;
#ifdef __windows__
    // Creating a new thread to handle timer logic
    os_thread_t timer_thread;
    os_thread_init(&timer_thread);

    TimeHandler *pTimeHandler = new TimeHandler(timer, handler);

    ret = os_thread_exec(&timer_thread, win_set_timer, (void *)pTimeHandler);
    if (ret) {
        printf("ERROR: win_set_timer() failed\n");
        return -1;
    }
#else
    os_set_signal_action(SIGALRM, handler);
    ret = setitimer(ITIMER_REAL, &timer, NULL);
    if (ret) {
        printf("ERROR: setitimer() failed\n");
        return -1;
    }
#endif
    return 0;
}

void os_set_disarm_timer(const itimerval& timer) {
#ifdef __windows__
    if (SetTimer(NULL, 0, 0, NULL) == 0) {
        printf("ERROR: SetTimer() failed when disarming");
    }
#else
    if (setitimer(ITIMER_REAL, &timer, NULL)) {
        printf("ERROR: setitimer() failed when disarming");
    }
#endif

}


const char* os_get_error(int res) {
#ifdef __windows__
    return gai_strerrorA(res);
#else
    return gai_strerror(res);
#endif
}

void os_unlink_unix_path(char* path) {
#ifdef __windows__
    remove(path);
#else
    unlink(path);
#endif
}

#ifdef __windows__
// Meny: This function contains test duration timer logic
void *win_set_timer(void *_pTimeHandler) {
    TimeHandler *pTimeHandler = (TimeHandler *)_pTimeHandler;
    itimerval timer = pTimeHandler->myTimer;

    HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
    if (!hTimer) printf("ERROR: Create timer failed\n");

    LARGE_INTEGER liDueTime;

    // Create an integer that will be used to signal the timer
    __int64 qwDueTime = -(((timer.it_value.tv_sec) * _SECOND) + (timer.it_value.tv_usec) * 10);

    // Copy the relative time into a LARGE_INTEGER.
    liDueTime.LowPart = (DWORD)(qwDueTime & 0xFFFFFFFF);
    liDueTime.HighPart = (LONG)(qwDueTime >> 32);

    int bSuccess = SetWaitableTimer(hTimer,     // Handle to the timer object
                                    &liDueTime, // When timer will become signaled
                                    0,          // Periodic timer
                                    0,          // Completion routine
                                    NULL,       // Argument to the completion routine
                                    FALSE);     // Do not restore a suspended system

    if (WaitForSingleObject(hTimer, INFINITE) != WAIT_OBJECT_0)
        printf("WaitForSingleObject failed (%d)\n", GetLastError());

    pTimeHandler->handler(SIGALRM);

    CloseHandle(hTimer);

    return 0;
}
#endif

bool os_sock_startup() {
#ifdef __windows__
    WSADATA wsaData;
    if (WSAStartup(0x202, &wsaData) != 0) {
        return false;
    }
    return true;
#else
    return true;
#endif
}

bool os_sock_cleanup() {
#ifdef __windows__
    if (WSACleanup() != 0) {
        return false;
    }
    return true;
#else
    return true;
#endif
}

void os_init_cpuset(os_cpuset_t *_mycpuset) {
#ifdef __windows__
    _mycpuset->cpuset = 0;
#else
    CPU_ZERO(&_mycpuset->cpuset);
#endif
}

void os_cpu_set(os_cpuset_t *_mycpuset, long _cpu_from, long _cpu_cur) {

    while ((_cpu_from <= _cpu_cur)) {
#ifdef __windows__
        _mycpuset->cpuset = (int)(1 << _cpu_from);
#else
        CPU_SET(_cpu_from, &(_mycpuset->cpuset));
#endif
        _cpu_from++;
    }
}

int os_set_affinity(const os_thread_t &thread, const os_cpuset_t &_mycpuset) {
#ifdef __windows__
    if (0 == SetThreadAffinityMask(thread.hThread, _mycpuset.cpuset)) return -1;
#else
    // Can't use thread.tid since it's syscall and not pthread_t
    if (0 != pthread_setaffinity_np(pthread_self(), sizeof(os_cpuset_t), &(_mycpuset.cpuset)))
        return -1;
#endif
    return 0;
}

int os_get_max_active_fds_num() {
#ifdef __windows__
    return MAX_OPEN_FILES;
#else
    static int max_active_fd_num = 0;
    if (!max_active_fd_num) {
        struct rlimit curr_limits;
        if (getrlimit(RLIMIT_NOFILE, &curr_limits) == -1) {
            perror("getrlimit");
            return 1024; // try the common default
        }
        max_active_fd_num = (int)curr_limits.rlim_max;
    }
    return max_active_fd_num;
#endif
}
