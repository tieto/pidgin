
EXTRA_DIST = gaim_applet.gnorba gaim_applet.desktop buddytrans

if GNOMEAPPLET

gaim_GNORBA = gaim_applet.gnorba
gaimgnorbadir  = $(sysconfdir)/CORBA/servers
gaimgnorba_DATA = $(gaim_GNORBA)

gaim_DESKTOP = gaim_applet.desktop
gaimdesktopdir = $(datadir)/applets/Network
gaimdesktop_DATA = $(gaim_DESKTOP)

endif

SUBDIRS = m4 libfaim sounds pixmaps doc src
ACLOCAL_AMFLAGS = -I m4
