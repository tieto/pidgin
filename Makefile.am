EXTRA_DIST = \
	gaim.spec.in gaim.desktop \
	Doxyfile.in \
	HACKING \
	gaim2blt.pl licq2gaim.pl setup-gettext \
	Makefile.mingw README.mingw config.h.mingw

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

appsdir = $(datadir)/applications
apps_DATA = gaim.desktop

SUBDIRS = sounds plugins pixmaps doc po intl src

docs: Doxyfile.in
	@doxygen
