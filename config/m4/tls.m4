# tls.m4 - Library to operate TLS
# 
# Copyright (C) Mellanox Technologies Ltd. 2020-2024.  ALL RIGHTS RESERVED.
# See file LICENSE for terms.
#

##########################
# tls usage support
#
AC_DEFUN([TLS_CAPABILITY_SETUP],
[
AC_ARG_WITH([tls],
    AS_HELP_STRING([--with-tls(=DIR)],
                   [Search for tls headers and libraries in DIR (default NO)]),
    [],
    [with_tls=no]
)

sockperf_cv_tls_lib=0
sockperf_cv_tls_lib_str="None"

AS_IF([test "x$with_tls" == xno],
    [],
    [
    if test -z "$with_tls" || test "$with_tls" = "yes"; then
        with_tls=/usr
    fi

    # Find a name of tls library as openssl, gnutls etc
    if test ! -d "openssl"; then
        sockperf_cv_tls_lib=1
        sockperf_cv_tls_lib_str="openssl"

        sockperf_cv_tls_save_CPPFLAGS="$CPPFLAGS"
        sockperf_cv_tls_save_CFLAGS="$CFLAGS"
        sockperf_cv_tls_save_LDFLAGS="$LDFLAGS"
        sockperf_cv_tls_save_LIBS="$LIBS"

        sockperf_cv_tls_CPPFLAGS="-I$with_tls/include"
        sockperf_cv_tls_LIBS="-lssl -lcrypto"
        sockperf_cv_tls_LDFLAGS="-L$with_tls/lib -Wl,--rpath,$with_tls/lib"
        if test -d "$with_tls/lib64"; then
            sockperf_cv_tls_LDFLAGS="-L$with_tls/lib64 -Wl,--rpath,$with_tls/lib64"
        fi

        CPPFLAGS="$sockperf_cv_tls_CPPFLAGS $CPPFLAGS"
        LDFLAGS="$sockperf_cv_tls_LDFLAGS $LDFLAGS"
        LIBS="$sockperf_cv_tls_LIBS $LIBS"

        AC_LANG_PUSH([C])
        AC_CHECK_HEADER(
            [openssl/ssl.h],
            [AC_LINK_IFELSE([AC_LANG_PROGRAM(
                [[#include <openssl/ssl.h>
                  #include <openssl/err.h>
                ]],
                [[SSL_library_init();
                  ERR_clear_error();
                  SSL_load_error_strings();
                ]])],
                [], [sockperf_cv_tls_lib=0])
            ],
            [sockperf_cv_tls_lib=0])
        AC_LANG_POP()

        CPPFLAGS="$sockperf_cv_tls_save_CPPFLAGS"
        CFLAGS="$sockperf_cv_tls_save_CFLAGS"
        LDFLAGS="$sockperf_cv_tls_save_LDFLAGS"
        LIBS="$sockperf_cv_tls_save_LIBS"
    fi
])

AC_MSG_CHECKING([for tls support])
if test "$sockperf_cv_tls_lib" -ne 0; then
    CPPFLAGS="$CPPFLAGS $sockperf_cv_tls_CPPFLAGS"
    LDFLAGS="$LDFLAGS $sockperf_cv_tls_LDFLAGS"
    LIBS="$LIBS $sockperf_cv_tls_LIBS"
    AC_DEFINE_UNQUOTED([DEFINED_TLS], [$sockperf_cv_tls_lib], [Using TLS])
    AC_MSG_RESULT([$sockperf_cv_tls_lib_str])
else
    AC_MSG_RESULT([no])
    AS_IF([test "x$with_tls" != xno],
        [AC_MSG_ERROR([TLS support was requested via --with-tls but the OpenSSL headers/libraries could not be found or failed the link test in "$with_tls"; install the OpenSSL development files or pass --with-tls=DIR with the correct prefix])])
fi
])
