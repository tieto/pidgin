EXTRA_DIST = \
	gaim.spec.in gaim.desktop \
	Doxyfile.in \
	HACKING \
	gaim2blt.pl licq2gaim.pl setup-gettext

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

pixmapdir = $(datadir)/applications
pixmap_DATA = gaim.desktop

SUBDIRS = sounds plugins pixmaps doc po intl src

docs: Doxyfile.in
	@doxygen
