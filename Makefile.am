
EXTRA_DIST = gaim.spec.in gaim_applet.gnorba gaim.desktop gaim_applet.desktop gaim.soundlist \
	buddytrans buddytrans2 README.plugins HACKING

if GNOMEAPPLET

gnomesysconf = `@GNOME_CONFIG@ --sysconfdir`
gnomedata    = `@GNOME_CONFIG@ --datadir`

gaim_GNORBA = gaim_applet.gnorba
gaimgnorbadir  = $(gnomesysconf)/CORBA/servers
gaimgnorba_DATA = $(gaim_GNORBA)

gaim_DESKTOP = gaim_applet.desktop
gaimdesktopdir = $(gnomedata)/applets/Network
gaimdesktop_DATA = $(gaim_DESKTOP)

gaim_SOUND = gaim.soundlist
gaimsounddir = $(gnomesysconf)/sound/events
gaimsound_DATA = $(gaim_SOUND)

endif

if DISTRIB

distribgnorbadir = $(sysconfdir)/CORBA/servers
distribgnorba_DATA = gaim_applet.gnorba

distribdesktopdir = $(datadir)/applets/Network
distribdesktop_DATA = gaim_applet.desktop

distribsounddir = $(sysconfdir)/sound/events
distribsound_DATA = gaim.soundlist

endif

# we used to have gnome-configurable sound events, but now we can configure them
# ourselves. isn't that lovely
#
#if GNOMEBITS
#
#bitssysconf = `@GNOME_CONFIG@ --sysconfdir`
#bitssounddir = $(bitssysconf)/sound/events
#bitssound_DATA = gaim.soundlist
#
#endif

pixmapdir = $(datadir)/gnome/apps/Internet
pixmap_DATA = gaim.desktop

SUBDIRS = m4 libfaim sounds plugins pixmaps doc po intl src
ACLOCAL_AMFLAGS = -I m4
