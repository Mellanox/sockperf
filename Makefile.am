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
	src/client.cpp \
	src/client.h \
	src/clock.h \
	src/common.cpp \
	src/common.h \
	src/defs.cpp \
	src/defs.h \
	src/input_handlers.h \
	src/iohandlers.cpp \
	src/iohandlers.h \
	src/ip_address.cpp \
	src/ip_address.h \
	src/message.cpp \
	src/message.h \
	src/message_parser.h \
	src/os_abstract.cpp \
	src/os_abstract.h \
	src/packet.cpp \
	src/packet.h \
	src/playback.cpp \
	src/playback.h \
	src/port_descriptor.h \
	src/server.cpp \
	src/server.h \
	src/sockperf.cpp \
	src/switches.h \
	src/ticks.cpp \
	src/ticks.h \
	src/ticks_os.h \
	src/tls.cpp \
	src/tls.h \
	src/vma-xlio-redirect.cpp \
	src/vma-redirect.h

dist_doc_DATA = \
	README.md \
	authors \
	news \
	version \
	copying

EXTRA_DIST = \
	build \
	contrib \
	debian \
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
