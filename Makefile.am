EXTRA_DIST = \
	gaim.spec.in gaim.desktop \
	Doxyfile.in \
	HACKING \
	gaim2blt.pl licq2gaim.pl setup-gettext

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

pixmapdir = $(datadir)/applications
pixmap_DATA = gaim.desktop

SUBDIRS = m4 sounds plugins pixmaps doc po intl src
ACLOCAL_AMFLAGS = -I m4

docs: Doxyfile.in
	@doxygen
