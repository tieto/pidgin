
EXTRA_DIST = gaim.spec.in gaim_applet.gnorba gaim.desktop gaim_applet.desktop \
	HACKING STATUS gaim2blt.pl

if DISTRIB

distribgnorbadir = $(sysconfdir)/CORBA/servers
distribgnorba_DATA = gaim_applet.gnorba

distribdesktopdir = $(datadir)/applets/Network
distribdesktop_DATA = gaim_applet.desktop

else

if GNOMEAPPLET

gnomesysconf = `@GNOME_CONFIG@ --sysconfdir`
gnomedata    = `@GNOME_CONFIG@ --datadir`

gaim_GNORBA = gaim_applet.gnorba
gaimgnorbadir  = $(gnomesysconf)/CORBA/servers
gaimgnorba_DATA = $(gaim_GNORBA)

gaim_DESKTOP = gaim_applet.desktop
gaimdesktopdir = $(gnomedata)/applets/Network
gaimdesktop_DATA = $(gaim_DESKTOP)

endif

endif

pixmapdir = $(datadir)/gnome/apps/Internet
pixmap_DATA = gaim.desktop

SUBDIRS = m4 sounds plugins pixmaps doc po intl src
ACLOCAL_AMFLAGS = -I m4
