SUBDIRS = protocols

bin_PROGRAMS = gaim
gaim_SOURCES =	about.c \
		aim.c \
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
		util.c
gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = @LDADD@ @LIBOBJS@

CFLAGS += $(PERL_CFLAGS)
CFLAGS += -DLOCALEDIR=\"$(datadir)/locale\" -DLIBDIR=\"$(libdir)/gaim/\" $(DEBUG_CFLAGS) -DDATADIR=\"$(datadir)\"

LIBS = @LIBS@ $(GTK_LIBS) $(SOUND_LIBS) $(STATIC_LINK_LIBS) $(PERL_LIBS) $(XSS_LIBS) $(INTLLIBS) $(DEBUG_LIBS)

EXTRA_DIST =	applet.h \
		convo.h \
		core.h \
		gaim.h \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtkimhtml.h \
		gtkspell.h \
		md5.h \
		multi.h \
		prpl.h \
		proxy.h \
		ui.h
