define([ver_major], esyscmd([sh -c "(awk -F. '{ print \$1 }' version) | tr -d '\n'"]))
define([ver_minor], esyscmd([sh -c "(awk -F. '{ print \$2 }' version) | tr -d '\n'"]))

AC_INIT([SOCKPERF], [ver_major.ver_minor], [sockperf-discuss@googlegroups.com])

AC_CONFIG_HEADER([config.h])
#AC_CONFIG_MACRO_DIR([config/m4])
AC_CONFIG_AUX_DIR([config/aux])

# Determine the host system
AC_CANONICAL_SYSTEM

TARGETDIR="unknown"
case "$host" in
 
  i?86-*-*) TARGET=X86; TARGETDIR=x86;;
  ia64*-*-*) TARGET=IA64; TARGETDIR=ia64;;
  powerpc*-*-linux* | powerpc-*-sysv*) TARGET=POWERPC; TARGETDIR=powerpc;;
  arm*-*-linux*) TARGET=ARM; TARGETDIR=arm;;
  powerpc-*-beos*) TARGET=POWERPC; TARGETDIR=powerpc;;
  powerpc-*-darwin*) TARGET=POWERPC_DARWIN; TARGETDIR=powerpc;;
  powerpc-*-aix* | rs6000-*-aix*) TARGET=POWERPC_AIX; TARGETDIR=powerpc;;
  powerpc-*-freebsd*) TARGET=POWERPC_FREEBSD; TARGETDIR=powerpc;;
  powerpc*-*-rtems*) TARGET=POWERPC; TARGETDIR=powerpc;;
  x86_64-*-*) TARGET=X86_64; TARGETDIR=x86;;
  amd64-*-freebsd*) TARGET=FREEBSD; TARGETDIR=x86;;
  i386-*-freebsd*) TARGET=FREEBSD; TARGETDIR=x86;;
  aarch64-*-*) TARGET=AARCH64; TARGETDIR=aarch64;;
  s390*-*-*) TARGET=S390; TARGETDIR=s390;;
esac
 
AC_SUBST(AM_RUNTESTFLAGS)
AC_SUBST(AM_LTLDFLAGS)
 
if test $TARGETDIR = unknown; then
  AC_MSG_ERROR(["it has not been ported to $host."])
fi
 
AM_CONDITIONAL(X86, test x$TARGET = xX86)
AM_CONDITIONAL(IA64, test x$TARGET = xIA64)
AM_CONDITIONAL(POWERPC, test x$TARGET = xPOWERPC)
AM_CONDITIONAL(ARM, test x$TARGET = xARM)
AM_CONDITIONAL(POWERPC_AIX, test x$TARGET = xPOWERPC_AIX)
AM_CONDITIONAL(POWERPC_DARWIN, test x$TARGET = xPOWERPC_DARWIN)
AM_CONDITIONAL(POWERPC_FREEBSD, test x$TARGET = xPOWERPC_FREEBSD)
AM_CONDITIONAL(FREEBSD, test x$TARGET = xFREEBSD)
AM_CONDITIONAL(AARCH64, test x$TARGET = xAARCH64)
AM_CONDITIONAL(S390, test x$TARGET = xS390)

AC_SUBST(TARGET)
AC_SUBST(TARGETDIR)

#########
# Locate a compiler for the build machine. This compiler should
# generate command-line programs that run on the build machine.
#
if test x"$cross_compiling" = xyes; then
    AC_MSG_NOTICE([Enable cross compiling on ${build_cpu} for ${host_cpu}])
#prefix=${prefix}/${host_cpu}
fi

AM_INIT_AUTOMAKE([foreign -Wall])
AM_MAINTAINER_MODE

#####################################
# check for C++ preprocessor and compiler
#
AC_PROG_CXX
#AC_PROG_LIBTOOL

##########################
# Documentation
#
AC_ARG_ENABLE(
    [doc],
    AC_HELP_STRING([--enable-doc],
                   [SOCKPERF: create documentation with doxygen in html and unix-man(default=yes)]),
    [], [enable_doc=no])

# Checks for doxygen
AC_PATH_PROG(DOXYGEN, doxygen, ,$PATH:/usr/local/bin:/usr/bin)
if test x$DOXYGEN = x; then
        enable_doc=no
fi
AM_CONDITIONAL(DOC, test x$enable_doc = xyes)



##########################
# Enable tests 
#
AC_ARG_ENABLE(
    [test],
    AC_HELP_STRING([--enable-test],
                   [SOCKPERF: compile tests (default=yes)]),
    [], [enable_test=no])
AM_CONDITIONAL(TEST, test x$enable_test = xyes)


##########################
# Enable tools 
#
AC_ARG_ENABLE(
    [tool],
    AC_HELP_STRING([--enable-tool],
                   [SOCKPERF: compile utilities (default=yes)]),
    [], [enable_tool=no])
AM_CONDITIONAL(TOOL, test x$enable_tool = xyes)


##########################
# Enable debug build
#
AC_ARG_ENABLE(
    [debug],
    AC_HELP_STRING([--enable-debug], 
                   [SOCKPERF: turn on debugging (default=no)]),
    [], [enable_debug=no])
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [], [Enable debugging])
    AC_DEFINE([_DEBUG], [], [Enable debugging])
else
    AC_DEFINE([NDEBUG], [], [Disable debugging])
fi

#####################################
# Checks for header files.
#
AC_HEADER_STDC
AC_CHECK_HEADERS(
    [arpa/inet.h fcntl.h netinet/in.h stdint.h stdlib.h \
    string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h \
    termios.h unistd.h])


#####################################
# Checks for libraries.
#
AC_HEADER_STDC
AC_SEARCH_LIBS([clock_gettime], [rt], [], AC_MSG_ERROR([librt not found]))
AC_SEARCH_LIBS([pthread_create], [pthread], [], AC_MSG_ERROR([libpthread not found]))
AC_SEARCH_LIBS([backtrace], [execinfo], [], AC_MSG_ERROR([libexecinfo not found]))
AC_SEARCH_LIBS([dlsym], [dl], [], AC_MSG_ERROR([libdl not found]))

AC_CONFIG_FILES([
    Makefile
    doc/Makefile
    src/Makefile
    tools/Makefile
    tests/Makefile
    ])

AC_OUTPUT

AC_MSG_RESULT([
	${PACKAGE} ${VERSION}

	prefix:		${prefix}

	compiler:	${CXX}
	cppflags:	${CPPFLAGS}
	cxxflags:	${CXXFLAGS}
	ldflags:	${LDFLAGS}

	doc:		${enable_doc}
	test:		${enable_test}
	tool:		${enable_tool}
	debug:		${enable_debug}
])
