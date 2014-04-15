EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		finch.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = finch.pc

SUBDIRS = libgnt plugins

if ENABLE_GNT

noinst_LTLIBRARIES = libfinch.la
bin_PROGRAMS = finch

endif

libfinch_la_SOURCES = \
	gntaccount.c \
	gntblist.c \
	gntcertmgr.c \
	gntconn.c \
	gntconv.c \
	gntdebug.c \
	gntidle.c \
	gntlog.c \
	gntmedia.c \
	gntmenuutil.c \
	gntnotify.c \
	gntplugin.c \
	gntpounce.c \
	gntprefs.c \
	gntrequest.c \
	gntroomlist.c \
	gntsound.c \
	gntstatus.c \
	gntui.c \
	gntxfer.c \
	libfinch.c

libfinch_la_headers = \
	gntaccount.h \
	gntblist.h \
	gntcertmgr.h \
	gntconn.h \
	gntconv.h \
	gntdebug.h \
	finch.h \
	gntidle.h \
	gntlog.h \
	gntmedia.h \
	gntmenuutil.h \
	gntnotify.h \
	gntplugin.h \
	gntpounce.h \
	gntprefs.h \
	gntrequest.h \
	gntroomlist.h \
	gntsound.h \
	gntstatus.h \
	gntui.h \
	gntxfer.h

finch_SOURCES = \
	finch.c

libfinchincludedir=$(includedir)/finch
libfinchinclude_HEADERS = \
	$(libfinch_la_headers)

libfinch_la_DEPENDENCIES = @LIBOBJS@
libfinch_la_LDFLAGS = -export-dynamic
libfinch_la_LIBADD = \
	@LIBOBJS@ \
	$(DBUS_LIBS) \
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(LIBXML_LIBS) \
	$(GNT_LIBS) \
	$(GSTREAMER_LIBS) \
	./libgnt/libgnt.la \
	$(top_builddir)/libpurple/libpurple.la

finch_DEPENDENCIES = $(builddir)/libfinch.la
finch_LDFLAGS = -export-dynamic
finch_LDADD = $(builddir)/libfinch.la $(libfinch_la_LIBADD)

AM_CPPFLAGS = \
	-DSTANDALONE \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_srcdir) \
	-I$(srcdir)/libgnt/ \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(GNT_CFLAGS)
