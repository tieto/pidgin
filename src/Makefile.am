EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c

SUBDIRS = protocols

bin_PROGRAMS = gaim gaim-remote
gaim_SOURCES =	about.c \
		away.c \
		browser.c \
		buddy.c \
		buddy_chat.c \
		buddyicondlg.c \
		buddyicondlg.h \
		conversation.c \
		conversation.h \
		core.c \
		core.h \
		dialogs.c \
		dnd-hints.c \
		dnd-hints.h \
		ft.c \
		ft.h \
		gaim.h \
		gaim-socket.h \
		gaim-disclosure.c \
		gaim-disclosure.h \
		gaimrc.c \
		gtkcellrendererprogress.c \
		gtkcellrendererprogress.h \
		gtkconv.c \
		gtkconv.h \
		gtkft.c \
		gtkft.h \
		gtkimhtml.c \
		gtkimhtml.h \
		gtkutils.c \
		gtkutils.h \
		html.c \
		idle.c \
		list.c \
		list.h
		log.c \
		main.c \
		md5.c \
		md5.h \
		module.c \
		multi.c \
		multi.h \
		perl.c \
		pounce.c \
		pounce.h \
		prefs.c \
		privacy.h \
		proxy.c \
		proxy.h \
		prpl.c \
		prpl.h \
		server.c \
		session.c \
		socket.c \
		sound.c \
		sound.h \
		stock.c \
		stock.h \
		themes.c \
		util.c \
		ui.h

gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = \
	@LIBOBJS@ \
	$(GTK_LIBS) \
	$(SOUND_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(PERL_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS)

gaim_remote_SOURCES = gaim-remote.c socket.c
gaim_remote_DEPENDENCIES = @LIBOBJS@
gaim_remote_LDADD = @LIBOBJS@ $(GLIB_LIBS)

AM_CPPFLAGS = \
	$(PERL_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(AUDIOFILE_CFLAGS) \
	$(AO_CFLAGS) \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)
