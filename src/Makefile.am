EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		win_gaim.c

SUBDIRS = protocols

CORESOURCES = \
	account.c \
	account.h \
	accountopt.c \
	accountopt.h \
	blist.c \
	blist.h \
	connection.c \
	connection.h \
	conversation.c \
	conversation.h \
	core.c \
	core.h \
	debug.c \
	debug.h \
	event.c \
	event.h \
	ft.c \
	ft.h \
	html.c \
	html.h \
	md5.c \
	md5.h \
	multi.h \
	notify.c \
	notify.h \
	plugin.c \
	plugin.h \
	pounce.c \
	pounce.h \
	privacy.h \
	proxy.c \
	proxy.h \
	prefs.c \
	prefs.h \
	prpl.c \
	prpl.h \
	request.c \
	request.h \
	server.c \
	server.h \
	status.c \
	status.h \
	sound.c \
	sound.h \
	util.c \
	util.h

bin_PROGRAMS = gaim gaim-remote
gaim_SOURCES = \
	$(CORESOURCES) \
	about.c \
	away.c \
	browser.c \
	buddy_chat.c \
	dialogs.c \
	dnd-hints.c \
	dnd-hints.h \
	gaim.h \
	gaim-disclosure.c \
	gaim-disclosure.h \
	gaimrc.c \
	gtkaccount.c \
	gtkaccount.h \
	gtkcellrendererprogress.c \
	gtkcellrendererprogress.h \
	gtkblist.c \
	gtkblist.h \
	gtkconn.c \
	gtkconn.h \
	gtkconv.c \
	gtkconv.h \
	gtkdebug.c \
	gtkdebug.h \
	gtkft.c \
	gtkft.h \
	gtkimhtml.c \
	gtkimhtml.h \
	gtknotify.c \
	gtknotify.h \
	gtkplugin.c \
	gtkplugin.h \
	gtkprefs.c \
	gtkprefs.h \
	gtkpounce.c \
	gtkpounce.h \
	gtkrequest.c \
	gtkrequest.h \
	gtksound.c \
	gtksound.h \
	gtkutils.c \
	gtkutils.h \
	idle.c \
	internal.h \
	log.c \
	log.h \
	main.c \
	session.c \
	stock.c \
	stock.h \
	themes.c \
	ui.h

gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = \
	@LIBOBJS@ \
	$(GTK_LIBS) \
	$(SOUND_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS)

gaim_remote_SOURCES = \
	gaim-remote.c

gaim_remote_DEPENDENCIES = @LIBOBJS@
gaim_remote_LDADD = \
	@LIBOBJS@ $(GLIB_LIBS) $(INTLLIBS) \
	-L$(top_builddir)/plugins/gaim-remote -lgaim-remote

AM_CPPFLAGS = \
	$(GTKSPELL_CFLAGS) \
	$(AUDIOFILE_CFLAGS) \
	$(AO_CFLAGS) \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS) \
	-I$(top_srcdir)/plugins
