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
			gtkspell.c \
			gtkticker.c \
			html.c \
			idle.c \
			multi.c \
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
gaim_applet_DEPENDENCIES = ../libfaim/libfaim.a @LIBOBJS@
gaim_applet_LDADD = @LDADD@ @LIBOBJS@
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
		gtkspell.c \
		gtkticker.c \
		html.c \
		idle.c \
		multi.c \
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
gaim_DEPENDENCIES = ../libfaim/libfaim.a @LIBOBJS@
gaim_LDADD = @LDADD@ @LIBOBJS@
endif

CFLAGS += -DLOCALEDIR=\"$(datadir)/locale\" -DLIBDIR=\"$(libdir)/gaim/\" $(DEBUG_CFLAGS)
LIBS += $(DEBUG_LIBS)

EXTRA_DIST =	convo.h \
		gaim.h \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtkhtml.h \
		gtkspell.h \
		gtkticker.h \
		gnome_applet_mgr.h \
		multi.h \
		prpl.h \
		proxy.h
