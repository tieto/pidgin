EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c

SUBDIRS = protocols

bin_PROGRAMS = gaim gaim-remote
gaim_SOURCES =	about.c \
		aim.c \
		away.c \
		browser.c \
		buddy.c \
		buddy_chat.c \
		conversation.c \
		convo.h \
		core.c \
		core.h \
		dialogs.c \
		ft.c \
		gaim.h \
		gaim-socket.h \
		gaimrc.c \
		gtkimhtml.c \
		gtkimhtml.h \
		html.c \
		idle.c \
		list.c \
		log.c \
		md5.c \
		md5.h \
		module.c \
		multi.c \
		multi.h \
		perl.c \
		prefs.c \
		proxy.c \
		proxy.h \
		prpl.c \
		prpl.h \
		server.c \
		session.c \
		socket.c \
		sound.c \
		util.c \
		ui.h

gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = \
	$(GTK_LIBS) \
	$(SOUND_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(PERL_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS)

gaim_remote_SOURCES = gaim-remote.c socket.c
gaim_remote_LDADD = $(GLIB_LIBS)

AM_CPPFLAGS = \
	$(PERL_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)

