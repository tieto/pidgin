EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		Makefile.mingw \
		win_gaim.c \
		win32/IdleTracker/Makefile.mingw \
		win32/IdleTracker/idletrack.c \
		win32/IdleTracker/idletrack.h \
		win32/MinimizeToTray.c \
		win32/MinimizeToTray.h \
		win32/gaimrc.rc \
		win32/global.mak \
		win32/libc_interface.c \
		win32/libc_interface.h \
		win32/libc_internal.h \
		win32/resource.h \
		win32/stdafx.h \
		win32/untar.c \
		win32/untar.h \
		win32/wgaimerror.h \
		win32/win32dep.c \
		win32/win32dep.h \
		win32/wspell.c \
		win32/wspell.h \
		win32/mingw_plus/winuser_extra.h \
		win32/nsis/gaim-header.bmp \
		win32/nsis/gaim-intro.bmp \
		win32/nsis/langmacros.nsh \
		win32/nsis/translations/bulgarian.nsh \
		win32/nsis/translations/czech.nsh \
		win32/nsis/translations/danish.nsh \
		win32/nsis/translations/dutch.nsh \
		win32/nsis/translations/english.nsh \
		win32/nsis/translations/finnish.nsh \
		win32/nsis/translations/french.nsh \
		win32/nsis/translations/german.nsh \
		win32/nsis/translations/hebrew.nsh \
		win32/nsis/translations/hungarian.nsh \
		win32/nsis/translations/italian.nsh \
		win32/nsis/translations/japanese.nsh \
		win32/nsis/translations/korean.nsh \
		win32/nsis/translations/polish.nsh \
		win32/nsis/translations/portuguese.nsh \
		win32/nsis/translations/portuguese-br.nsh \
		win32/nsis/translations/romanian.nsh \
		win32/nsis/translations/russian.nsh \
		win32/nsis/translations/serbian-latin.nsh \
		win32/nsis/translations/simp-chinese.nsh \
		win32/nsis/translations/spanish.nsh \
		win32/nsis/translations/swedish.nsh \
		win32/nsis/translations/trad-chinese.nsh


SUBDIRS = protocols

CORESOURCES = \
	account.c \
	account.h \
	accountopt.c \
	accountopt.h \
	blist.c \
	blist.h \
	buddyicon.c \
	buddyicon.h \
	connection.c \
	connection.h \
	conversation.c \
	conversation.h \
	core.c \
	core.h \
	debug.c \
	debug.h \
	eventloop.c \
	eventloop.h \
	ft.c \
	ft.h \
	imgstore.c \
	imgstore.h \
	log.c \
	log.h \
	md5.c \
	md5.h \
	multi.h \
	network.c \
	network.h \
	notify.c \
	notify.h \
	plugin.c \
	plugin.h \
	pounce.c \
	pounce.h \
	privacy.c \
	privacy.h \
	proxy.c \
	proxy.h \
	prefs.c \
	prefs.h \
	prpl.c \
	prpl.h \
	request.c \
	request.h \
	roomlist.c \
	roomlist.h \
	server.c \
	server.h \
	sha.c \
	sha.h \
	signals.c \
	signals.h \
	status.c \
	status.h \
	stringref.c \
	stringref.h \
	sound.c \
	sound.h \
	sslconn.c \
	sslconn.h \
	util.c \
	util.h \
	value.c \
	value.h \
	xmlnode.c \
	xmlnode.h

bin_PROGRAMS = gaim gaim-remote
gaim_SOURCES = \
	$(CORESOURCES) \
	about.c \
	away.c \
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
	gtkeventloop.c \
	gtkeventloop.h \
	gtkft.c \
	gtkft.h \
	gtkimhtml.c \
	gtkimhtml.h \
	gtkimhtmltoolbar.c \
	gtkimhtmltoolbar.h \
	gtkinternal.h \
	gtklog.c \
	gtklog.h \
	gtknotify.c \
	gtknotify.h \
	gtkplugin.c \
	gtkplugin.h \
	gtkprefs.c \
	gtkprefs.h \
	gtkprivacy.c \
	gtkprivacy.h \
	gtkpounce.c \
	gtkpounce.h \
	gtkrequest.c \
	gtkrequest.h \
	gtkroomlist.c \
	gtkroomlist.h \
	gtksound.c \
	gtksound.h \
	gtksourceiter.c \
	gtksourceiter.h \
	gtkutils.c \
	gtkutils.h \
	idle.c \
	internal.h \
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
	$(GTKSPELL_LIBS) \
	$(STARTUP_NOTIFICATION_LIBS)

gaim_remote_SOURCES = \
	gaim-remote.c

gaim_remote_DEPENDENCIES = @LIBOBJS@
gaim_remote_LDADD = \
	@LIBOBJS@ $(GLIB_LIBS) $(INTLLIBS) \
	$(top_builddir)/plugins/gaim-remote/libgaim-remote.la

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-I$(top_srcdir)/plugins \
	$(AUDIOFILE_CFLAGS) \
	$(AO_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS)
