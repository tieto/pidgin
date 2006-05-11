EXTRA_DIST = \
		dbus-analyze-functions.py \
		dbus-analyze-types.py \
		gaim-notifications-example \
		gaim-remote \
		gaim-send \
		gaim-send-async \
		getopt.c \
		getopt.h \
		getopt1.c \
		Makefile.mingw \
		win_gaim.c \
		win32/IdleTracker/Makefile.mingw \
		win32/IdleTracker/idletrack.c \
		win32/IdleTracker/idletrack.h \
		win32/gaimrc.rc \
		win32/global.mak \
		win32/libc_interface.c \
		win32/libc_interface.h \
		win32/libc_internal.h \
		win32/resource.h \
		win32/untar.c \
		win32/untar.h \
		win32/wgaimerror.h \
		win32/win32dep.c \
		win32/win32dep.h \
		win32/wspell.c \
		win32/wspell.h \
		win32/nsis/gaim-header.bmp \
		win32/nsis/gaim-intro.bmp \
		win32/nsis/gaim-plugin.nsh \
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
		win32/nsis/translations/kurdish.nsh \
		win32/nsis/translations/lithuanian.nsh \
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
	gaim_buffer.c \
	idle.c \
	imgstore.c \
	log.c \
	mime.c \
	network.c \
	ntlm.c \
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
	dnssrv.c\
	status.c \
	stringref.c \
	stun.c \
	sound.c \
	sslconn.c \
	upnp.c \
	util.c \
	value.c \
	xmlnode.c \
	whiteboard.c

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
	dbus-maybe.h \
	debug.h \
	desktopitem.h \
	eventloop.h \
	ft.h \
	gaim_buffer.h \
	idle.h \
	imgstore.h \
	log.h \
	mime.h \
	network.h \
	notify.h \
	ntlm.h \
	plugin.h \
	pluginpref.h \
	pounce.h \
	prefs.h \
	privacy.h \
	proxy.h \
	prpl.h \
	request.h \
	roomlist.h \
	savedstatuses.h \
	server.h \
	signals.h \
	dnssrv.h \
	status.h \
	stringref.h \
	stun.h \
	sound.h \
	sslconn.h \
	upnp.h \
	util.h \
	value.h \
	version.h \
	xmlnode.h \
	whiteboard.h

if ENABLE_DBUS

CLEANFILES = \
	dbus-bindings.c \
	dbus-client-binding.c \
	dbus-client-binding.h \
	dbus-types.c \
	dbus-types.h \
	gaim-client-bindings.c \
	gaim-client-bindings.h \
	gaim.service

# gaim dbus server

dbus_sources  = dbus-server.c dbus-useful.c
dbus_headers  = dbus-bindings.h dbus-gaim.h dbus-server.h dbus-useful.h dbus-define-api.h

dbus_exported = dbus-useful.h dbus-define-api.h account.h blist.h buddyicon.h connection.h conversation.h core.h log.h roomlist.h savedstatuses.h status.h server.h

gaim_build_coreheaders = $(addprefix $(srcdir)/, $(gaim_coreheaders))
dbus_build_exported = $(addprefix $(srcdir)/, $(dbus_exported))

dbus-types.c: dbus-analyze-types.py $(gaim_coreheaders)
	cat $(gaim_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --pattern=GAIM_DBUS_DEFINE_TYPE\(%s\) > $@

dbus-types.h: dbus-analyze-types.py $(dbus_coreheaders)
	cat $(gaim_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --pattern=GAIM_DBUS_DECLARE_TYPE\(%s\) > $@

dbus-bindings.c: dbus-analyze-functions.py $(dbus_exported)
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py > $@

dbus-server.$(OBJEXT): dbus-bindings.c dbus-types.c dbus-types.h
$(gaim_OBJECTS): dbus-types.h

# libgaim-client

lib_LTLIBRARIES = libgaim-client.la

libgaim_client_la_SOURCES = gaim-client.c gaim-client.h

libgaim_client_la_LIBADD = $(DBUS_LIBS)

gaim-client-bindings.c: dbus-analyze-functions.py $(dbus_exported)
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py --client > $@

gaim-client-bindings.h: dbus-analyze-types.py dbus-analyze-functions.py $(gaim_coreheaders) $(dbus_exported)
	cat $(gaim_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --keyword=enum --verbatim > $@
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py --client --headers >> $@

$(libgaim_client_la_OBJECTS): gaim-client-bindings.h gaim-client-bindings.c

# gaim-client-example

gaim_client_example_SOURCES = gaim-client-example.c

gaim_client_example_DEPENDENCIES = @LIBOBJS@ libgaim-client.la

gaim_client_example_LDADD = \
	@LIBOBJS@ \
	libgaim-client.la \
	$(GTK_LIBS) \
	$(DBUS_LIBS)

gaim_clientbP = gaim-client-example

gaim-client-example.$(OBJEXT): gaim-client-bindings.h

# scripts

bin_SCRIPTS = gaim-remote gaim-send gaim-send-async

exampledir = $(datadir)/doc/@PACKAGE@/examples
example_DATA = gaim-notifications-example

endif

bin_PROGRAMS = gaim $(gaim_clientbP)

gaim_SOURCES = \
	$(gaim_coresources) \
	$(dbus_sources) \
	gtkblist.c \
	gtkcombobox.c \
	gtkcelllayout.c \
	gtkcellview.c \
	gtkcellviewmenuitem.c \
	gtkaccount.c \
	gtkcellrendererprogress.c \
	gtkconn.c \
	gtkconv.c \
	gtkdebug.c \
	gtkdialogs.c \
	gtkdnd-hints.c \
	gtkeventloop.c \
	gtkexpander.c \
	gtkft.c \
	gtkidle.c \
	gtkimhtml.c \
	gtkimhtmltoolbar.c \
	gtklog.c \
	gtkmain.c \
	gtkmenutray.c \
	gtknotify.c \
	gtkplugin.c \
	gtkpluginpref.c \
	gtkprefs.c \
	gtkprivacy.c \
	gtkpounce.c \
	gtkrequest.c \
	gtkroomlist.c \
	gtksavedstatuses.c \
	gtksession.c \
	gtksound.c \
	gtksourceiter.c \
	gtkstatusbox.c \
	gtkstock.c \
	gtkthemes.c \
	gtkutils.c \
	gtkwhiteboard.c

gaim_headers = \
	$(dbus_headers) \
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
	gtkconvwin.h \
	gtkdebug.h \
	gtkdialogs.h \
	gtkdnd-hints.h \
	gtkeventloop.h \
	gtkexpander.h \
	gtkft.h \
	gtkgaim.h \
	gtkidle.h \
	gtkimhtml.h \
	gtkimhtmltoolbar.h \
	gtklog.h \
	gtkmenutray.h \
	gtknickcolors.h \
	gtknotify.h \
	gtkplugin.h \
	gtkpluginpref.h \
	gtkprefs.h \
	gtkprivacy.h \
	gtkpounce.h \
	gtkrequest.h \
	gtkroomlist.h \
	gtksavedstatuses.h \
	gtksession.h \
	gtksound.h \
	gtksourceiter.h \
	gtkstatusbox.h \
	gtkstock.h \
	gtkthemes.h \
	gtkutils.h \
	gtkwhiteboard.h

noinst_HEADERS= \
	internal.h \
	prefix.h

gaimincludedir=$(includedir)/gaim
gaiminclude_HEADERS = \
	$(gaim_coreheaders) \
	$(gaim_headers)


gaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS) $(MS_LIBS)
gaim_LDFLAGS = -export-dynamic
gaim_LDADD = \
	@LIBOBJS@ \
	$(GTK_LIBS) \
	$(DBUS_LIBS) \
	$(GSTREAMER_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(XSS_LIBS) \
	$(SM_LIBS) \
	$(INTLLIBS) \
	$(GTKSPELL_LIBS) \
	$(STARTUP_NOTIFICATION_LIBS) 

AM_CPPFLAGS = \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/plugins \
	$(GSTREAMER_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTKSPELL_CFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS)
