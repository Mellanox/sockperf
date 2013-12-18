#
# sockperf utility.
#
AM_LDFLAGS = -rdynamic

all:
	@(cp $(DESTDIR)$(bin_PROGRAMS) $(DESTDIR)$(bin_PROGRAMS)_$(PACKAGE_VERSION))

clean-local:
	@(rm $(DESTDIR)$(bin_PROGRAMS)_$(PACKAGE_VERSION))

install-data-local:
	@(cp $(bin_PROGRAMS)_$(PACKAGE_VERSION) $(DESTDIR)${prefix}/bin/. && cp $(bin_PROGRAMS) $(DESTDIR)${prefix}/bin/.)

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
	os_abstract.h \
	IoHandlers.h \
	Message.h \
	PacketTimes.h \
	Playback.h \
	Server.h \
	Ticks.h \
	ticks_os.h \
	vma-redirect.h

sockperf_SOURCES = \
	aopt.cpp \
	Client.cpp \
	common.cpp \
	Defs.cpp \
	os_abstract.cpp \
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
	os_abstract.h \
	IoHandlers.h \
	Message.h \
	PacketTimes.h \
	Playback.h \
	Server.h \
	Ticks.h \
	ticks_os.h \
	vma-redirect.h
