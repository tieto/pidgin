SUBDIRS = protocols

if GNOMEAPPLET
bin_PROGRAMS = gaim_applet
gaim_applet_SOURCES =	about.c \
			aim.c \
			applet.c \
			away.c \
			browser.c \
			buddy.c \
			buddy_chat.c \
			conversation.c \
			core.c \
			dialogs.c \
			gaimrc.c \
			gtkimhtml.c \
			gtkspell.c \
			gtkticker.c \
			html.c \
			idle.c \
			list.c \
			md5.c \
			module.c \
			multi.c \
			perl.c \
			plugins.c \
			prefs.c \
			proxy.c \
			prpl.c \
			server.c \
			sound.c \
			ticker.c \
			util.c
gaim_applet_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_applet_LDFLAGS = -export-dynamic
gaim_applet_LDADD = @LDADD@ @LIBOBJS@
else
bin_PROGRAMS = gaim
gaim_SOURCES =	about.c \
		aim.c \
		applet.c \
		away.c \
		browser.c \
		buddy.c \
		buddy_chat.c \
		conversation.c \
		core.c \
		dialogs.c \
		gaimrc.c \
		gtkimhtml.c \
		gtkspell.c \
		gtkticker.c \
		html.c \
		idle.c \
		list.c \
		md5.c \
		module.c \
		multi.c \
		perl.c \
		plugins.c \
		prefs.c \
		proxy.c \
		prpl.c \
		server.c \
		sound.c \
		ticker.c \
		util.c
gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = @LDADD@ @LIBOBJS@
endif

CFLAGS += $(PERL_CFLAGS)
CFLAGS += -DLOCALEDIR=\"$(datadir)/locale\" -DLIBDIR=\"$(libdir)/gaim/\" $(DEBUG_CFLAGS) -DDATADIR=\"$(datadir)\"

LIBS = @LIBS@ $(UI_LIBS) $(SOUND_LIBS) $(STATIC_LINK_LIBS) $(PERL_LIBS) $(XSS_LIBS) $(INTLLIBS) $(DEBUG_LIBS)

EXTRA_DIST =	applet.h \
		convo.h \
		core.h \
		gaim.h \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtkimhtml.h \
		gtkspell.h \
		gtkticker.h \
		md5.h \
		multi.h \
		prpl.h \
		proxy.h \
		ui.h
