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
		win32/nsis/translations/albanian.nsh \
		win32/nsis/translations/bulgarian.nsh \
		win32/nsis/translations/catalan.nsh \
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
		win32/nsis/translations/norwegian.nsh \
		win32/nsis/translations/polish.nsh \
		win32/nsis/translations/portuguese.nsh \
		win32/nsis/translations/portuguese-br.nsh \
		win32/nsis/translations/romanian.nsh \
		win32/nsis/translations/russian.nsh \
		win32/nsis/translations/serbian-latin.nsh \
		win32/nsis/translations/simp-chinese.nsh \
		win32/nsis/translations/slovak.nsh \
		win32/nsis/translations/slovenian.nsh \
		win32/nsis/translations/spanish.nsh \
		win32/nsis/translations/swedish.nsh \
		win32/nsis/translations/trad-chinese.nsh \
		win32/nsis/translations/vietnamese.nsh


SUBDIRS = protocols

gaim_coresources = \
	account.c \
	accountopt.c \
	blist.c \
	buddyicon.c \
	cmds.c \
	connection.c \
	conversation.c \
	core.c \
	debug.c \
	desktopitem.c \
	eventloop.c \
	ft.c \
	imgstore.c \
	log.c \
	md5.c \
	network.c \
	notify.c \
	plugin.c \
	pluginpref.c \
	pounce.c \
	prefix.c \
	prefs.c \
	privacy.c \
	proxy.c \
	prpl.c \
	request.c \
	roomlist.c \
	savedstatuses.c \
	server.c \
	sha.c \
	signals.c \
	status.c \
	stringref.c \
	sound.c \
	sslconn.c \
	util.c \
	value.c \
	xmlnode.c

gaim_coreheaders = \
	account.h \
	accountopt.h \
	blist.h \
	buddyicon.h \
	cmds.h \
	connection.h \
	conversation.h \
	core.h \
	debug.h \
	desktopitem.h \
	eventloop.h \
	ft.h \
	imgstore.h \
	log.h \
	md5.h \
	network.h \
	notify.h \
	plugin.h \
	pluginpref.h \
	pounce.h \
	prefix.h \
	prefs.h \
	privacy.h \
	proxy.h \
	prpl.h \
	request.h \
	roomlist.h \
	savedstatuses.h \
	server.h \
	sha.h \
	signals.h \
	status.h \
	stringref.h \
	sound.h \
	sslconn.h \
	util.h \
	value.h \
	version.h \
	xmlnode.h

bin_PROGRAMS = gaim gaim-remote

gaim_SOURCES = \
	$(gaim_coresources) \
	gtkaccount.c \
	gtkcellrendererprogress.c \
	gtkblist.c \
	gtkconn.c \
	gtkconv.c \
	gtkdebug.c \
	gtkdialogs.c \
	gtkdnd-hints.c \
	gtkgaim-disclosure.c \
	gtkeventloop.c \
	gtkft.c \
	gtkimhtml.c \
	gtkimhtmltoolbar.c \
	gtklog.c \
	gtkmain.c \
	gtknotify.c \
	gtkplugin.c \
	gtkpluginpref.c \
	gtkprefs.c \
	gtkprivacy.c \
	gtkpounce.c \
	gtkrequest.c \
	gtkroomlist.c \
	gtksavedstatuses.c \
	gtksound.c \
	gtksourceiter.c \
	gtkstatusselector.c \
	gtkstock.c \
	gtkthemes.c \
	gtkutils.c \
	idle.c \
	session.c

gaim_headers = \
	gaim.h \
	gtkaccount.h \
	gtkcellrendererprogress.h \
	gtkblist.h \
	gtkconn.h \
	gtkconv.h \
	gtkdebug.h \
	gtkdialogs.h \
	gtkdnd-hints.h \
	gtkeventloop.h \
	gtkft.h \
	gtkgaim.h \
	gtkgaim-disclosure.h \
	gtkimhtml.h \
	gtkimhtmltoolbar.h \
	gtklog.h \
	gtknotify.h \
	gtkplugin.h \
	gtkpluginpref.h \
	gtkprefs.h \
	gtkprivacy.h \
	gtkpounce.h \
	gtkrequest.h \
	gtkroomlist.h \
	gtksavedstatuses.h \
	gtksound.h \
	gtksourceiter.h \
	gtkstatusselector.h \
	gtkstock.h \
	gtkutils.h \
	internal.h

gaimincludedir=$(includedir)/gaim
gaiminclude_HEADERS = \
	$(gaim_coreheaders) \
	$(gaim_headers)

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
	gaim-remote.c \
	prefix.c \
	prefix.h

gaim_remote_DEPENDENCIES = @LIBOBJS@
gaim_remote_LDADD = \
	@LIBOBJS@ $(GLIB_LIBS) $(INTLLIBS) \
	$(top_builddir)/plugins/gaim-remote/libgaim-remote.la

AM_CPPFLAGS = \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/plugins \
	$(AUDIOFILE_CFLAGS) \
	$(AO_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS)
