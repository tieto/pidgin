EXTRA_DIST = gaim.xpm admin_icon.xpm aimicon.xpm \
	aimicon2.xpm aol_icon.xpm big.xpm \
	bold.xpm dt_icon.xpm free_icon.xpm \
	italic.xpm login_icon.xpm logo.xpm \
	logout_icon.xpm no_icon.xpm normal.xpm \
	link.xpm small.xpm speaker.xpm im.xpm \
        buddychat.xpm info.xpm buddyadd.xpm \
        buddydel.xpm permadd.xpm permdel.xpm \
        wood.xpm palette.xpm offline.xpm \
        online.xpm signing_on.xpm msg_pending.xpm \
        away.xpm away_icon.xpm underline.xpm \
        peng_smile.xpm peng_frown.xpm peng_wink.xpm \
	strike.xpm gnome/devil-connect.png gnome/devil-offline.png \
	gnome/devil-online.png gnome/penguin-connect.png \
	gnome/penguin-offline.png gnome/penguin-online.png \
	daemon-buddyadd.xpm daemon-buddychat.xpm daemon-buddydel.xpm \
	daemon-im.xpm daemon-info.xpm daemon-permadd.xpm \
	daemon-permdel.xpm daemon.xpm fontface.xpm smile_happy.xpm \
	smile_sad.xpm smile_wink.xpm save.xpm cancel.xpm

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = gaim.xpm
	
if GNOMEAPPLET
gnomesysconf = `@GNOME_CONFIG@ --sysconfdir`
gnomedata    = `@GNOME_CONFIG@ --datadir`
gaimpixmapdir = $(gnomedata)/pixmaps
gaimpixmap_DATA = gaim.xpm
gaimgnomedir = $(gnomedata)/pixmaps/gaim/gnome
gaimgnome_DATA = 
gaimgnomepixmapsdir = $(gnomedata)/pixmaps/gaim
gaimgnomepixmaps_DATA = gnome/penguin-online.png gnome/penguin-offline.png \
	gnome/penguin-connect.png gnome/devil-offline.png \
	gnome/devil-connect.png gnome/devil-online.png
endif
