if GNOMEAPPLET
bin_PROGRAMS = gaim_applet
gaim_applet_SOURCES =	about.c \
			aim.c \
			away.c \
			browser.c \
			buddy.c \
			buddy_chat.c \
			conversation.c \
			dialogs.c \
			gaimrc.c \
			gnome_applet_mgr.c \
			gtkhtml.c \
			gtkticker.c \
			html.c \
			idle.c \
			multi.c \
			network.c \
			oscar.c \
			perl.c \
			plugins.c \
			prefs.c \
			proxy.c \
			prpl.c \
			rvous.c \
			server.c \
			sound.c \
			ticker.c \
			toc.c \
			util.c
gaim_applet_DEPENDENCIES = ../libfaim/libfaim.a
else
bin_PROGRAMS = gaim
gaim_SOURCES =	about.c \
		aim.c \
		away.c \
		browser.c \
		buddy.c \
		buddy_chat.c \
		conversation.c \
		dialogs.c \
		gaimrc.c \
		gnome_applet_mgr.c \
		gtkhtml.c \
		gtkticker.c \
		html.c \
		idle.c \
		multi.c \
		network.c \
		oscar.c \
		perl.c \
		plugins.c \
		prefs.c \
		proxy.c \
		prpl.c \
		rvous.c \
		server.c \
		sound.c \
		ticker.c \
		toc.c \
		util.c
gaim_DEPENDENCIES = ../libfaim/libfaim.a
endif

CFLAGS += -DLOCALEDIR=\"$(datadir)/locale\" $(DEBUG_CFLAGS)
LDFLAGS += $(DEBUG_LDFLAGS)

EXTRA_DIST = gaim.h proxy.h gnome_applet_mgr.h gtkhtml.h gtkticker.h convo.h multi.h prpl.h
