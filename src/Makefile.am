if GNOMEAPPLET
bin_PROGRAMS = gaim_applet
gaim_applet_SOURCES = about.c aim.c away.c browser.c buddy.c buddy_chat.c \
	conversation.c dialogs.c gaimrc.c gnome_applet_mgr.c gtkhtml.c \
	html.c idle.c network.c oscar.c plugins.c prefs.c proxy.c rvous.c \
	server.c sound.c toc.c util.c
else
bin_PROGRAMS = gaim
gaim_SOURCES = about.c aim.c away.c browser.c buddy.c buddy_chat.c \
	conversation.c dialogs.c gaimrc.c gnome_applet_mgr.c gtkhtml.c \
	html.c idle.c network.c oscar.c plugins.c prefs.c proxy.c rvous.c \
	server.c sound.c toc.c util.c
endif

EXTRA_DIST = gaim.h proxy.h gnome_applet_mgr.h gtkhtml.h
