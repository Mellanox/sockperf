noinst_PROGRAMS = gtest

#CXXFLAGS =
# google test shows some warnings that are suppressed
AM_CXXFLAGS = \
	-Wno-error=sign-compare \
	-Wno-error=missing-field-initializers \
	-g -O0

AM_CPPFLAGS = \
	-DGTEST_LANG_CXX11=0 \
	-DGTEST_HAS_PTHREAD=1 \
	-DGTEST_USE_OWN_TR1_TUPLE=0 \
	-DGTEST_HAS_TR1_TUPLE=1 \
	-DGTEST_ENV_HAS_STD_TUPLE_=0 \
	-DGTEST_USES_SIMPLE_RE=0 \
	-DPTHREADS \
	\
	-DGTEST_DONT_DEFINE_ASSERT_GT=0 \
	-DGTEST_DONT_DEFINE_ASSERT_GE=0 \
	-DGTEST_DONT_DEFINE_ASSERT_LT=0 \
	-DGTEST_DONT_DEFINE_ASSERT_LE=0 \
	-DGTEST_DONT_DEFINE_ASSERT_NE=0 \
	-DGTEST_DONT_DEFINE_ASSERT_EQ=0 \
	-DGTEST_DONT_DEFINE_SUCCEED=0 \
	-DGTEST_DONT_DEFINE_FAIL=0 \
	-DGTEST_DONT_DEFINE_TEST=0 \
	\
	-DGTEST_HAS_STD_WSTRING=0 \
	-DGTEST_HAS_GLOBAL_STRING=0 \
	\
	-DGTEST_OS_WINDOWS=0 \
	-DGTEST_OS_LINUX_ANDROID=0 \
	-DGTEST_OS_CYGWIN=0 \
	-DGTEST_OS_SOLARIS=0 \
	-DGTEST_OS_SYMBIAN=0 \
	-DGTEST_OS_WINDOWS_MOBILE=0 \
	-DGTEST_OS_QNX=0 \
	-DGTEST_OS_MAC=0 \
	-DGTEST_OS_IOS=0 \
	-DGTEST_OS_ZOS

# lgtest
noinst_LTLIBRARIES = libgtest.la

libgtest_la_CPPFLAGS = \
	-I$(top_srcdir)/tests/gtest/googletest \
	-I$(top_srcdir)/tests/gtest/googletest/include \
	$(AM_CPPFLAGS)

libgtest_la_LDFLAGS = -pthread -static
libgtest_la_CXXFLAGS = \
	$(AM_CXXFLAGS)

EXTRA_DIST = \
	googletest/include \
	googletest/src \
	googletest/README.md \
	googletest/LICENSE

libgtest_la_SOURCES = \
	googletest/src/gtest-all.cc \
	googletest/src/gtest_main.cc

# gtest
gtest_LDADD = libgtest.la

gtest_CPPFLAGS = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/tests/gtest/googletest/include \
	$(AM_CPPFLAGS)

gtest_LDFLAGS = -no-install
gtest_CXXFLAGS = \
	$(AM_CXXFLAGS)

gtest_SOURCES = \
	main.cpp \
	\
	message_parser_tests.cpp

noinst_HEADERS =

gtest_DEPENDENCIES = \
	libgtest.la

# This workaround allows to compile files located
# at another directory.
# This place resolve make distcheck issue
nodist_gtest_SOURCES = \
	defs.cpp \
	message.cpp \
	os_abstract.cpp

CLEANFILES = \
	defs.cpp \
	message.cpp \
	os_abstract.cpp

defs.cpp:
	@echo "#include \"$(top_builddir)/src/$@\"" >$@

message.cpp:
	@echo "#include \"$(top_builddir)/src/$@\"" >$@

os_abstract.cpp:
	@echo "#include \"$(top_builddir)/src/$@\"" >$@
