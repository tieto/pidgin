
EXTRA_DIST = gaim.spec gaim_applet.gnorba gaim_applet.desktop gaim_applet.soundlist \
	buddytrans plugins/CRAZY plugins/HOWTO plugins/Makefile plugins/SIGNALS \
	plugins/autorecon.c plugins/chkmail.c plugins/filectl.c plugins/gaiminc.c \
	plugins/iconaway.c plugins/simple.c plugins/spellchk.c README.plugins

if GNOMEAPPLET

gnomesysconf = `@GNOME_CONFIG@ --sysconfdir`
gnomedata    = `@GNOME_CONFIG@ --datadir`

gaim_GNORBA = gaim_applet.gnorba
gaimgnorbadir  = $(gnomesysconf)/CORBA/servers
gaimgnorba_DATA = $(gaim_GNORBA)

gaim_DESKTOP = gaim_applet.desktop
gaimdesktopdir = $(gnomedata)/applets/Network
gaimdesktop_DATA = $(gaim_DESKTOP)

gaim_SOUND = gaim_applet.soundlist
gaimsounddir = $(gnomesysconf)/sound/events
gaimsound_DATA = $(gaim_SOUND)

endif

SUBDIRS = m4 libfaim sounds pixmaps doc src
ACLOCAL_AMFLAGS = -I m4
