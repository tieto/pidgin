EXTRA_DIST = gaim.spec.in gaim.desktop \
	HACKING gaim2blt.pl licq2gaim.pl setup-gettext

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

pixmapdir = $(datadir)/gnome/apps/Internet
pixmap_DATA = gaim.desktop

SUBDIRS = m4 sounds plugins pixmaps doc po intl src
ACLOCAL_AMFLAGS = -I m4
