
EXTRA_DIST = gaim_applet.gnorba gaim_applet.desktop gaim_applet.soundlist \
	buddytrans

if GNOMEAPPLET

gaim_GNORBA = gaim_applet.gnorba
gaimgnorbadir  = $(sysconfdir)/CORBA/servers
gaimgnorba_DATA = $(gaim_GNORBA)

gaim_DESKTOP = gaim_applet.desktop
gaimdesktopdir = $(datadir)/applets/Network
gaimdesktop_DATA = $(gaim_DESKTOP)

gaim_SOUND = gaim_applet.soundlist
gaimsounddir = $(sysconfdir)/sound/events
gaimsound_DATA = $(gaim_SOUND)

endif

SUBDIRS = m4 libfaim sounds pixmaps doc src
ACLOCAL_AMFLAGS = -I m4
