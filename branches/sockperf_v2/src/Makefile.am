#
# sockperf utility.
#
AM_LDFLAGS = -rdynamic

all:
	@(cp $(bin_PROGRAMS) $(bin_PROGRAMS)_$(PACKAGE_VERSION))

clean-local:
	@(rm $(bin_PROGRAMS)_$(PACKAGE_VERSION))

install-data-local:
	@(cp $(bin_PROGRAMS)_$(PACKAGE_VERSION) ${prefix}/bin/. && cp $(bin_PROGRAMS) ${prefix}/bin/.)

bin_PROGRAMS = sockperf
INCLUDES := \
	-I.


noinst_HEADERS = \
	aopt.h \
	clock.h \
	Switches.h \
	Client.h \
	common.h \
	Defs.h \
	IoHandlers.h \
	Message.h \
	PacketTimes.h \
	Playback.h \
	Server.h \
	Ticks.h \
	vma-redirect.h

sockperf_SOURCES = \
	aopt.c \
	Client.cpp \
	common.cpp \
	Defs.cpp \
	IoHandlers.cpp \
	Message.cpp \
	PacketTimes.cpp \
	Playback.cpp \
	Server.cpp \
	Ticks.cpp \
	vma-redirect.cpp \
	SockPerf.cpp

EXTRA_DIST = \
	aopt.h \
	clock.h \
	Switches.h \
	Client.h \
	common.h \
	Defs.h \
	IoHandlers.h \
	Message.h \
	PacketTimes.h \
	Playback.h \
	Server.h \
	Ticks.h \
	vma-redirect.h
