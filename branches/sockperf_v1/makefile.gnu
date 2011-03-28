#
# Make targets
# Author: Igor Ivanov Igor.Ivanov@itseez.com 
#

include gnumake.def


export OUT_LOG=$(TOP_DIR)/gnu_out.log
export WRN_LOG=$(TOP_DIR)/gnu_wrn.log

SUBDIRS = src

all: prepare
	for i in $(SUBDIRS); do $(MAKE) -s -C $$i -f makefile.gnu || exit 1; done

install: prepare
	for i in src; do $(MAKE) -s -C $$i -f makefile.gnu install || exit 1; done

uninstall: prepare
	for i in src; do $(MAKE) -s -C $$i -f makefile.gnu uninstall || exit 1; done

clean: prepare
	for i in $(SUBDIRS); do $(MAKE) -s -C $$i -f makefile.gnu clean; done

dist: clean
	@echo BUILD : Package preparation [$@]
	@tar cf - -C . `for n in . doc src tools tests; do find $$n -type f -name '*'; done` | \
                  gzip -9 >vperf-${VERSION}.tar.gz

prepare:
	$(DEL) -f $(OUT_LOG)
	$(DEL) -f $(WRN_LOG)