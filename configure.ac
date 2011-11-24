define([ver_major], esyscmd([sh -c "(awk -F. '{ print \$1 }' version) | tr -d '\n'"]))
define([ver_minor], esyscmd([sh -c "(awk -F. '{ print \$2 }' version) | tr -d '\n'"]))
define([ver_svn], esyscmd([sh -c "(svnversion .) | awk '{ print \$1 }' | tr -d '\n'"]))

AC_INIT([SOCKPERF], [ver_major.ver_minor.ver_svn], [sockperf-discuss@googlegroups.com])

AC_DEFINE(SOCKPERF_MAJOR_VER, ver_major, [sockperf Major Version])
AC_DEFINE(SOCKPERF_MINOR_VER, ver_minor, [sockperf Minor Version])
AC_DEFINE(SOCKPERF_MICRO_VER, ver_svn, [sockperf Micro Version])
VERSION=ver_major.ver_minor.ver_svn


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
  powerpc-*-beos*) TARGET=POWERPC; TARGETDIR=powerpc;;
  powerpc-*-darwin*) TARGET=POWERPC_DARWIN; TARGETDIR=powerpc;;
  powerpc-*-aix* | rs6000-*-aix*) TARGET=POWERPC_AIX; TARGETDIR=powerpc;;
  powerpc-*-freebsd*) TARGET=POWERPC_FREEBSD; TARGETDIR=powerpc;;
  powerpc*-*-rtems*) TARGET=POWERPC; TARGETDIR=powerpc;;
  x86_64-*-*) TARGET=X86_64; TARGETDIR=x86;;
esac
 
AC_SUBST(AM_RUNTESTFLAGS)
AC_SUBST(AM_LTLDFLAGS)
 
if test $TARGETDIR = unknown; then
  AC_MSG_ERROR(["it has not been ported to $host."])
fi
 
AM_CONDITIONAL(X86, test x$TARGET = xX86)
AM_CONDITIONAL(IA64, test x$TARGET = xIA64)
AM_CONDITIONAL(POWERPC, test x$TARGET = xPOWERPC)
AM_CONDITIONAL(POWERPC_AIX, test x$TARGET = xPOWERPC_AIX)
AM_CONDITIONAL(POWERPC_DARWIN, test x$TARGET = xPOWERPC_DARWIN)
AM_CONDITIONAL(POWERPC_FREEBSD, test x$TARGET = xPOWERPC_FREEBSD)

AC_SUBST(TARGET)
AC_SUBST(TARGETDIR)


AC_MSG_NOTICE([ ])
AC_MSG_NOTICE([release $VERSION])

#########
# Locate a compiler for the build machine. This compiler should
# generate command-line programs that run on the build machine.
#
if test x"$cross_compiling" = xyes; then
    AC_MSG_NOTICE([Enable cross compiling on ${build_cpu} for ${host_cpu}])
#prefix=${prefix}/${host_cpu}
fi

AC_MSG_NOTICE([..............................................])


AM_INIT_AUTOMAKE(foreign)
AM_MAINTAINER_MODE

#####################################
# check for C++ preprocessor and compiler
#
AC_PROG_CXX
#AC_PROG_LIBTOOL


CXXFLAGS="-Werror -Wall  --param inline-unit-growth=200"
CXXFLAGS="$CXXFLAGS  -D__LINUX__  -DVERSION=$VERSION"


##########################
# Documentation
#
AC_ARG_ENABLE(
    [doc],
    AC_HELP_STRING([--enable-doc],
                   [SOCKPERF: create documentation with doxygen in html and unix-man(default=yes)]))

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
                   [SOCKPERF: compile tests (default=yes)]))
AM_CONDITIONAL(TEST, test x$enable_test = xyes)


##########################
# Enable tools 
#
AC_ARG_ENABLE(
    [tool],
    AC_HELP_STRING([--enable-tool],
                   [SOCKPERF: compile utilities (default=yes)]))
AM_CONDITIONAL(TOOL, test x$enable_tool = xyes)


##########################
# Enable debug build
#
AC_ARG_ENABLE(
    [debug],
    AC_HELP_STRING([--enable-debug], 
                   [SOCKPERF: turn on debugging (default=no)]))
if test x$enable_debug = xyes; then
    TARGET_BUILD=dbg
    CXXFLAGS="$CXXFLAGS -g -D_DEBUG -DDEBUG"
	CFLAGS="$CFLAGS -g -O3"
else
    TARGET_BUILD=rls
    CXXFLAGS="$CXXFLAGS -O3 -DNDEBUG"
	CFLAGS="$CFLAGS -O3"
fi


##########################
# Enable debug info
#
AC_ARG_ENABLE(
    [debuginfo],
    AC_HELP_STRING([--enable-debuginfo],
                   [SOCKPERF: include debug information (default=no)]))
if test x$enable_debuginfo = xyes; then
    CXXFLAGS="$CXXFLAGS -g"
	CFLAGS="$CFLAGS -g"
fi

##########################
# Enable build 32bit
#
AC_ARG_ENABLE(
	[build32],
	AC_HELP_STRING([--enable-build32],
					[SOCKPERF: force build 32bit (default=no)]))
if test x$enable_build32 = xyes; then
	    CXXFLAGS="$CXXFLAGS -m32"
		CFLAGS="$CFLAGS -m32"
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
AC_CHECK_LIB(
    [rt], [clock_gettime], [], AC_MSG_ERROR([librt not found]))
AC_CHECK_LIB(
    [pthread], [pthread_create], [], AC_MSG_ERROR([libpthread not found]))
AC_CHECK_LIB(
    [dl], [dlsym], [], AC_MSG_ERROR([libdl not found]))


RPM_RELEASE=1
AC_SUBST(RPM_RELEASE)
RPM_DIR=${top_sockperf_dir}
AC_SUBST(RPM_DIR)
RPM_TARBALL=${PACKAGE}-${VERSION}.tar.gz
AC_SUBST(RPM_TARBALL)


OUT_DIR=`pwd`/bin/linux/${TARGET_BUILD}/${host_cpu}
#AC_PREFIX_DEFAULT("`pwd`/inst")


AC_CONFIG_FILES([
    Makefile
    doc/Makefile
    src/Makefile
    tools/Makefile
    tests/Makefile
    ])

AC_OUTPUT

