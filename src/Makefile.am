SUBDIRS = protocols

bin_PROGRAMS = gaim gaim-remote
gaim_SOURCES =	about.c \
		aim.c \
		away.c \
		browser.c \
		buddy.c \
		buddy_chat.c \
		conversation.c \
		core.c \
		dialogs.c \
		ft.c \
		gaimrc.c \
		gtkimhtml.c \
		html.c \
		idle.c \
		list.c \
		md5.c \
		module.c \
		multi.c \
		perl.c \
		prefs.c \
		proxy.c \
		prpl.c \
		server.c \
		session.c \
		socket.c \
		sound.c \
		util.c
gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = @LDADD@ @LIBOBJS@

gaim_remote_LDADD = @LDADD@ @LIBOBJS@
gaim_remote_SOURCES = gaim-remote.c socket.c

CFLAGS += $(PERL_CFLAGS) $(GTKSPELL_CFLAGS)
CFLAGS += -DLOCALEDIR=\"$(datadir)/locale\" -DLIBDIR=\"$(libdir)/gaim/\" $(DEBUG_CFLAGS) -DDATADIR=\"$(datadir)\"

LIBS = @LIBS@ $(GTK_LIBS) $(SOUND_LIBS) $(STATIC_LINK_LIBS) $(PERL_LIBS) $(XSS_LIBS) $(SM_LIBS) $(INTLLIBS) $(DEBUG_LIBS) $(GTKSPELL_LIBS)

EXTRA_DIST =	convo.h \
		core.h \
		gaim.h \
		gaim-socket.h \
		getopt.c \
		getopt.h \
		getopt1.c \
		gtkimhtml.h \
		md5.h \
		multi.h \
		prpl.h \
		proxy.h \
		ui.h
