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
	$(mkinstalldirs) ${DESTDIR}$(docdir)
	cp -rf man/man/ ${DESTDIR}$(prefix)/share/
	cp -rf man/html/ ${DESTDIR}$(docdir)
	cp -rf ${docfiles} ${DESTDIR}$(docdir)

uninstall-local:
	$(RM) -rf ${DESTDIR}$(docdir)/*

clean-local:
	$(RM) -r man doxygen.log


