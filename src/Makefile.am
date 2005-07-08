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
	cipher.c \
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
	mime.c \
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
	cipher.h \
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
	mime.h \
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


if ENABLE_DBUS

gaim_coresources  += dbus-server.c 
gaim_coreheaders  += dbus-server.h

# do not use gaim-client, use gaim-send instead
# bin_PROGRAMS      += gaim-client
#gaim_client_SOURCES      = dbus-client.c
#gaim_client_DEPENDENCIES = @LIBOBJS@
#gaim_client_LDADD        = @LIBOBJS@ $(GLIB_LIBS) $(DBUS_LIBS) $(INTLLIBS)

dbus-client-bindings.c: dbus-service.xml
	dbus-binding-tool --prefix=gaim_object --mode=glib-client --output=$@ $<

dbus-server-bindings.c: dbus-service.xml
	dbus-binding-tool --prefix=gaim_object --mode=glib-server --output=$@ $<

dbus-client.$(OBJEXT): dbus-client-bindings.c
dbus-server.$(OBJEXT): dbus-server-bindings.c

endif

gaim_SOURCES = \
	$(gaim_coresources) \
	gtkcombobox.c \
	gtkcelllayout.c \
	gtkcellview.c \
	gtkcellviewmenuitem.c \
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
	gtkstatusbox.c \
	gtkstock.c \
	gtkthemes.c \
	gtkutils.c \
	idle.c \
	session.c

gaim_headers = \
	gaim.h \
	gtkcombobox.h \
	gtkcelllayout.h \
	gtkcellview.h \
	gtkcellviewmenuitem.h \
	gtkcellview.h \
	gtkcellviewmenuitem.h \
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
	gtkstatusbox.h \
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
	$(DBUS_LIBS) \
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
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS)
