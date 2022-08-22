define([sockperf_version], esyscmd([sh -c "head -1 ./build/current-version | tr -d '\n'"]))
AC_INIT([SOCKPERF], [sockperf_version], [http://github.com/mellanox/sockperf/issues])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([config/m4])
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

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AM_MAINTAINER_MODE

#####################################
# check for C++ preprocessor and compiler
#
: ${CXXFLAGS="-O3 -g"}
AC_PROG_CXX
AC_LANG(C++)
AM_PROG_AR
AC_PROG_LIBTOOL

AX_CXX_COMPILE_STDCXX([11], [], [mandatory])

SP_CHECK_CXXFLAGS_APPEND([OUR_CXXFLAGS], [\
                         -Wall \
                         "--param inline-unit-growth=300"])

##########################################################################
# check VMA extra API
#
AC_ARG_ENABLE(
    [vma-api],
    AC_HELP_STRING([--enable-vma-api],
                   [SOCKPERF: enable vma extra api support: 'yes', 'no' or library installation path (default=no)]),
    [have_vma_api=$enableval],
    [have_vma_api=no])
AS_IF([test "${have_vma_api}" != "no"],
    [
    if test "$have_vma_api" = "yes"
    then
        have_vma_api=/usr
    fi
    CPPFLAGS="$CPPFLAGS -I$have_vma_api/include"

    AC_CHECK_HEADER([mellanox/vma_extra.h],
        [AC_DEFINE([USING_VMA_EXTRA_API],[1],[[Enable using VMA extra API]])],
        [AC_MSG_ERROR([vma_extra.h file not found at $have_vma_api/include])]
        [have_vma_api=no])])
AC_MSG_CHECKING(
    [for vma extra api])
AC_MSG_RESULT([${have_vma_api}])

##########################################################################
# check XLIO extra API
#
AC_ARG_ENABLE(
    [xlio-api],
    AC_HELP_STRING([--enable-xlio-api],
                   [SOCKPERF: enable xlio extra api support: 'yes', 'no' or library installation path (default=no)]),
    [have_xlio_api=$enableval],
    [have_xlio_api=no])
AS_IF([test "${have_xlio_api}" != "no"],
    [
    if test "$have_xlio_api" = "yes"
    then
        have_xlio_api=/usr
    fi
    CPPFLAGS="$CPPFLAGS -I$have_xlio_api/include"

    AC_CHECK_HEADER([mellanox/xlio_extra.h],
        [AC_DEFINE([USING_XLIO_EXTRA_API],[1],[[Enable using XLIO extra API]])],
        [AC_MSG_ERROR([xlio_extra.h file not found at $have_xlio_api/include])]
        [have_xlio_api=no])])
AC_MSG_CHECKING(
    [for xlio extra api])
AC_MSG_RESULT([${have_xlio_api}])

##########################
# Documentation
#
SP_ARG_ENABLE_BOOL(
    [doc],
    AC_HELP_STRING([--enable-doc],
                   [SOCKPERF: create documentation with doxygen in html and unix-man (default=no)]))
AS_IF([test "x$have_doc" = "xyes"], [
       # Checks for doxygen
       AC_PATH_PROG(DOXYGEN, doxygen, ,$PATH:/usr/local/bin:/usr/bin)
       AS_IF([test -z "$DOXYGEN"],
	     [AC_MSG_ERROR([Building of doc requested, but doxygen not found])])])
AM_CONDITIONAL(DOC, test "x$have_doc" = "xyes")


##########################
# Enable tests 
#
SP_ARG_ENABLE_BOOL(
    [test],
    AC_HELP_STRING([--enable-test],
                   [SOCKPERF: compile tests (default=no)]))
AM_CONDITIONAL(TEST, test "x$have_test" = "xyes")


##########################
# Enable tools 
#
SP_ARG_ENABLE_BOOL(
    [tool],
    AC_HELP_STRING([--enable-tool],
                   [SOCKPERF: compile utilities (default=no)]))
AM_CONDITIONAL(TOOL, test "x$have_tool" = "xyes")


##########################
# Enable debug build
#
SP_ARG_ENABLE_BOOL(
    [debug],
    AC_HELP_STRING([--enable-debug],
                   [SOCKPERF: turn on debugging (default=no)]))
AS_IF([test "x$have_debug" = "xyes"],
      [AC_DEFINE([DEBUG],  [], [Enable debugging])],
      [AC_DEFINE([NDEBUG], [], [Disable debugging])])


##########################
# Enable build 32bit
#
SP_ARG_ENABLE_BOOL(
    [build32],
    AC_HELP_STRING([--enable-build32],
		   [SOCKPERF: force build 32bit (default=no)]))
AS_IF([test "x$have_build32" = "xyes"], [
      SP_CHECK_CXXFLAG_APPEND([OUR_CXXFLAGS], [-m32],
                              [AC_MSG_ERROR([32bit build requested, but -m32 compiler flag does not work])])])

AC_SUBST([OUR_CXXFLAGS])

##########################
# TLS support
#
TLS_CAPABILITY_SETUP

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
    tools/Makefile
    tests/Makefile
    tests/gtest/Makefile
    doc/Doxyfile
    build/sockperf.spec
    ])

AC_OUTPUT

AC_MSG_RESULT([
	${PACKAGE} ${VERSION}

	prefix:		${prefix}

	compiler:	${CXX}
	cppflags:	${CPPFLAGS}
	cxxflags:	${OUR_CXXFLAGS} ${CXXFLAGS}
	ldflags:	${LDFLAGS}

	doc:		${have_doc}
	test:		${have_test}
	tool:		${have_tool}
	vma_api:	${have_vma_api}
	xlio_api:	${have_xlio_api}
	debug:		${have_debug}
])
