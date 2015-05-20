EXTRA_DIST = \
	Doxyfile.in \
	main.dox

man_MANS = man/man/man3/sockperf.3

.PHONY: doxygen
doxygen: man/man/man3/sockperf.3

man/man/man3/sockperf.3: Doxyfile $(top_srcdir)/doc/main.dox
	doxygen Doxyfile

install-data-local:
	$(mkinstalldirs) ${DESTDIR}$(docdir)
	cp -rf man/html/ ${DESTDIR}$(docdir)

uninstall-local:
	rm -rf ${DESTDIR}$(docdir)/html

clean-local:
	rm -rf man doxygen.log
