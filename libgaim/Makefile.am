EXTRA_DIST = \
		dbus-analyze-functions.py \
		dbus-analyze-types.py \
		gaim-notifications-example \
		gaim-remote \
		gaim-send \
		gaim-send-async \
		gaim-url-handler \
		Makefile.mingw \
		win32/global.mak \
		win32/libc_interface.c \
		win32/libc_interface.h \
		win32/libc_internal.h \
		win32/libgaimrc.rc.in \
		win32/rules.mak \
		win32/targets.mak \
		win32/wgaimerror.h \
		win32/win32dep.c \
		win32/giowin32.c \
		win32/win32dep.h

if USE_GCONFTOOL
GCONF_DIR=gconf
endif

SUBDIRS = $(GCONF_DIR) plugins protocols

gaim_coresources = \
	account.c \
	accountopt.c \
	blist.c \
	buddyicon.c \
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
	xmlnode.c \
	whiteboard.c

gaim_coreheaders = \
	account.h \
	accountopt.h \
	blist.h \
	buddyicon.h \
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
	gaim.h \
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

dbus_exported = dbus-useful.h dbus-define-api.h account.h blist.h buddyicon.h \
                connection.h conversation.h core.h log.h prefs.h roomlist.h \
                savedstatuses.h status.h server.h xmlnode.h

gaim_build_coreheaders = $(addprefix $(srcdir)/, $(gaim_coreheaders))
dbus_build_exported = $(addprefix $(srcdir)/, $(dbus_exported))

dbus-types.c: dbus-analyze-types.py $(gaim_coreheaders)
	cat $(gaim_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --pattern=GAIM_DBUS_DEFINE_TYPE\(%s\) > $@

dbus-types.h: dbus-analyze-types.py $(dbus_coreheaders)
	cat $(gaim_build_coreheaders) | $(PYTHON) $(srcdir)/dbus-analyze-types.py --pattern=GAIM_DBUS_DECLARE_TYPE\(%s\) > $@

dbus-bindings.c: dbus-analyze-functions.py $(dbus_exported)
	cat $(dbus_build_exported) | $(PYTHON) $(srcdir)/dbus-analyze-functions.py > $@

dbus-server.$(OBJEXT): dbus-bindings.c dbus-types.c dbus-types.h
dbus-server.lo: dbus-bindings.c dbus-types.c dbus-types.h
$(libgaim_la_OBJECTS): dbus-types.h

# libgaim-client

libgaim_client_lib = libgaim-client.la

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

gaim_client_example_DEPENDENCIES = libgaim-client.la

gaim_client_example_LDADD = \
	libgaim-client.la \
	$(GLIB_LIBS) \
	$(DBUS_LIBS)

bin_PROGRAMS = gaim-client-example

gaim-client-example.$(OBJEXT): gaim-client-bindings.h

# scripts

bin_SCRIPTS = gaim-remote gaim-send gaim-send-async gaim-url-handler

endif

lib_LTLIBRARIES = libgaim.la $(libgaim_client_lib)

libgaim_la_SOURCES = \
	$(gaim_coresources) \
	$(dbus_sources)

noinst_HEADERS= \
	internal.h \
	prefix.h

libgaimincludedir=$(includedir)/gaim
libgaiminclude_HEADERS = \
	$(gaim_coreheaders) \
	$(dbus_headers)

libgaim_la_DEPENDENCIES = $(STATIC_LINK_LIBS)
libgaim_la_LDFLAGS = -export-dynamic
libgaim_la_LIBADD = \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(LIBXML_LIBS) \
	$(LIBNM_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(INTLLIBS) \
	-lm

AM_CPPFLAGS = \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(LIBNM_CFLAGS)
