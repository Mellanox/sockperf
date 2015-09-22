if TEST
TEST_SUBDIR = tests
endif

if TOOL
TOOL_SUBDIR = tools
endif

SUBDIRS = ${TEST_SUBDIR} ${TOOL_SUBDIR}

ACLOCAL_AMFLAGS = -I config/m4
AM_LDFLAGS = -rdynamic
AM_CPPFLAGS = -imacros $(builddir)/config.h
AM_CXXFLAGS = $(OUR_CXXFLAGS)

bin_PROGRAMS = sockperf

sockperf_SOURCES = \
	src/aopt.cpp \
	src/aopt.h \
	src/Client.cpp \
	src/Client.h \
	src/clock.h \
	src/common.cpp \
	src/common.h \
	src/Defs.cpp \
	src/Defs.h \
	src/IoHandlers.cpp \
	src/IoHandlers.h \
	src/Message.cpp \
	src/Message.h \
	src/os_abstract.cpp \
	src/os_abstract.h \
	src/PacketTimes.cpp \
	src/PacketTimes.h \
	src/Playback.cpp \
	src/Playback.h \
	src/Server.cpp \
	src/Server.h \
	src/SockPerf.cpp \
	src/Switches.h \
	src/Ticks.cpp \
	src/Ticks.h \
	src/ticks_os.h \
	src/vma-redirect.cpp \
	src/vma-redirect.h

dist_doc_DATA = \
	readme \
	authors \
	news \
	version \
	copying

EXTRA_DIST = \
	doc/Doxyfile.in \
	doc/main.dox

if DOC
.PHONY: doxygen

doxygen: doc/man/man/man3/sockperf.3

doc/man/man/man3/sockperf.3: doc/Doxyfile $(srcdir)/doc/main.dox
	doxygen doc/Doxyfile

install-data-local:
	$(mkinstalldirs) ${DESTDIR}$(docdir)
	cp -rf doc/man/html/ ${DESTDIR}$(docdir)

uninstall-local:
	rm -rf ${DESTDIR}$(docdir)/html

clean-local:
	rm -rf doc/man doc/doxygen.log

man_MANS = doc/man/man/man3/sockperf.3

endif
