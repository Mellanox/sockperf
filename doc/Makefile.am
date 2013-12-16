docdir   = ${DESTDIR}$(prefix)/share/sockperf/
docfiles = \
	$(top_srcdir)/readme \
	$(top_srcdir)/authors \
	$(top_srcdir)/news \
	$(top_srcdir)/version \
	$(top_srcdir)/copying

EXTRA_DIST = \
	Doxyfile \
	main.dox

.PHONY: doxygen

targets = doxygen

all-local: $(targets)

doxygen: Doxyfile main.dox
	( cat Doxyfile ; echo "PROJECT_NUMBER=`cat ../version`" ) | doxygen -

install-data-local:
	$(mkinstalldirs) $(docdir)
	cp -rf man/man/ ${DESTDIR}$(prefix)/share/man/
	cp -rf man/* $(docdir)
	cp -rf ${docfiles} $(docdir)

uninstall-local:
	$(RM) -rf $(docdir)/*

clean-local:
	$(RM) -r man doxygen.log


