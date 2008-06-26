EXTRA_DIST = \
		dbus-analyze-functions.py \
		dbus-analyze-types.py \
		purple-notifications-example \
		purple-remote \
		purple-send \
		purple-send-async \
		purple-url-handler \
		purple.h.in \
		purple.pc.in \
		purple-uninstalled.pc.in \
		version.h.in \
		Makefile.mingw \
		win32/global.mak \
		win32/libc_interface.c \
		win32/libc_interface.h \
		win32/libc_internal.h \
		win32/libpurplerc.rc.in \
		win32/rules.mak \
		win32/targets.mak \
		win32/wpurpleerror.h \
		win32/win32dep.c \
		win32/giowin32.c \
		win32/win32dep.h

if USE_GCONFTOOL
GCONF_DIR=gconf
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = purple.pc

SUBDIRS = $(GCONF_DIR) plugins protocols tests . example

purple_coresources = \
	account.c \
	accountopt.c \
	blist.c \
	buddyicon.c \
	certificate.c \
	cipher.c \
	circbuffer.c \
	cmds.c \
	connection.c \
	conversation.c \
	core.c \
	debug.c \
	desktopitem.c \
	eventloop.c \
	ft.c \
	idle.c \
	imgstore.c \
	log.c \
	mime.c \
	nat-pmp.c \
	network.c \
	ntlm.c \
	notify.c \
	plugin.c \
	pluginpref.c \
	pounce.c \
	prefs.c \
	privacy.c \
	proxy.c \
	prpl.c \
	request.c \
	roomlist.c \
	savedstatuses.c \
	server.c \
	signals.c \
	smiley.c \
	dnsquery.c \
	dnssrv.c\
	status.c \
	stringref.c \
	stun.c \
	sound.c \
	sslconn.c \
	upnp.c \
	util.c \
	value.c \
	version.c \
	xmlnode.c \
	whiteboard.c

purple_coreheaders = \
	account.h \
	accountopt.h \
	blist.h \
	buddyicon.h \
	certificate.h \
	cipher.h \
	circbuffer.h \
	cmds.h \
	connection.h \
	conversation.h \
	core.h \
	dbus-maybe.h \
	debug.h \
	desktopitem.h \
	eventloop.h \
	ft.h \
	gaim-compat.h \
	idle.h \
	imgstore.h \
	log.h \
	mime.h \
	nat-pmp.h \
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
	smiley.h \
	dnsquery.h \
	dnssrv.h \
	status.h \
	stringref.h \
	stun.h \
	sound.h \
	sslconn.h \
	upnp.h \
	util.h \
	value.h \
	xmlnode.h \
	whiteboard.h

purple_builtheaders = purple.h version.h

if ENABLE_DBUS

CLEANFILES = \
	dbus-bindings.c \
	dbus-client-binding.c \
	dbus-client-binding.h \
	dbus-types.c \
	dbus-types.h \
	purple-client-bindings.c \
	purple-client-bindings.h \
	purple.service

# purple dbus server

dbus_sources  = dbus-server.c dbus-useful.c
dbus_headers  = dbus-bindings.h dbus-purple.h dbus-server.h dbus-useful.h dbus-define-api.h dbus-types.h

dbus_exported = dbus-useful.h dbus-define-api.h account.h blist.h buddyicon.h \
                connection.h conversation.h core.h ft.h log.h notify.h prefs.h roomlist.h \
                savedstatuses.h smiley.h status.h server.h util.h xmlnode.h prpl.h

purple_build_coreheaders = $(addprefix $(srcdir)/, $(purple_coreheaders)) \
		$(purple_builtheaders)
dbus_build_exported = $(addprefix $(srcdir)/, $(dbus_exported))

dbus-types.c: dbus-analyze-types.py $(purple_build_coreheaders)
	cat $(purple_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --pattern=PURPLE_DBUS_DEFINE_TYPE\(%s\) > $@

dbus-types.h: dbus-analyze-types.py $(purple_build_coreheaders)
	cat $(purple_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --pattern=PURPLE_DBUS_DECLARE_TYPE\(%s\) > $@

dbus-bindings.c: dbus-analyze-functions.py $(dbus_exported)
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py > $@

dbus-server.$(OBJEXT): dbus-bindings.c dbus-types.c dbus-types.h
dbus-server.lo: dbus-bindings.c dbus-types.c dbus-types.h
$(libpurple_la_OBJECTS): dbus-types.h

# libpurple-client

libpurple_client_lib = libpurple-client.la

libpurple_client_la_SOURCES = purple-client.c purple-client.h
libpurple_client_la_LDFLAGS = -version-info $(PURPLE_LT_VERSION_INFO) -no-undefined
libpurple_client_la_LIBADD = $(DBUS_LIBS)

purple-client-bindings.c: dbus-analyze-functions.py $(dbus_exported)
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py --client > $@

purple-client-bindings.h: dbus-analyze-types.py dbus-analyze-functions.py $(purple_coreheaders) $(purple_builtheaders) $(dbus_exported)
	cat $(purple_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --keyword=enum --verbatim > $@
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py --client --headers >> $@

$(libpurple_client_la_OBJECTS): purple-client-bindings.h purple-client-bindings.c

# purple-client-example

purple_client_example_SOURCES = purple-client-example.c

purple_client_example_DEPENDENCIES = libpurple-client.la

purple_client_example_LDADD = \
	libpurple-client.la \
	$(GLIB_LIBS) \
	$(DBUS_LIBS)

bin_PROGRAMS = purple-client-example

purple-client-example.$(OBJEXT): purple-client-bindings.h

# scripts

bin_SCRIPTS = purple-remote purple-send purple-send-async purple-url-handler

BUILT_SOURCES = $(purple_builtheaders) \
	dbus-types.c \
	dbus-types.h \
	dbus-bindings.c \
	purple-client-bindings.c \
	purple-client-bindings.h

else

BUILT_SOURCES = $(purple_builtheaders)

endif

lib_LTLIBRARIES = libpurple.la $(libpurple_client_lib)

libpurple_la_SOURCES = \
	$(purple_coresources) \
	$(dbus_sources)

noinst_HEADERS= \
	internal.h 

libpurpleincludedir=$(includedir)/libpurple
libpurpleinclude_HEADERS = \
	$(purple_coreheaders) \
	$(purple_builtheaders) \
	$(dbus_headers)

libpurple_la_DEPENDENCIES = $(STATIC_LINK_LIBS)
libpurple_la_LDFLAGS = -export-dynamic -version-info $(PURPLE_LT_VERSION_INFO) -no-undefined
libpurple_la_LIBADD = \
	$(STATIC_LINK_LIBS) \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(LIBXML_LIBS) \
	$(NETWORKMANAGER_LIBS) \
	$(INTLLIBS) \
	-lm

AM_CPPFLAGS = \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/purple-$(PURPLE_MAJOR_VERSION)/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(NETWORKMANAGER_CFLAGS)

# INSTALL_SSL_CERTIFICATES is true when SSL_CERTIFICATES_DIR is empty.
# We want to use SSL_CERTIFICATES_DIR when it's not empty.
if ! INSTALL_SSL_CERTIFICATES
AM_CPPFLAGS += -DSSL_CERTIFICATES_DIR=\"$(SSL_CERTIFICATES_DIR)\"
endif
