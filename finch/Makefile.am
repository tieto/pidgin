if ENABLE_GNT

EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c

SUBDIRS = libgnt plugins

bin_PROGRAMS = finch

finch_SOURCES = \
	gntaccount.c \
	gntblist.c \
	gntconn.c \
	gntconv.c \
	gntdebug.c \
	gntft.c \
	finch.c \
	gntidle.c \
	gntnotify.c \
	gntplugin.c \
	gntpounce.c \
	gntprefs.c \
	gntrequest.c \
	gntstatus.c \
	gntui.c

finch_headers = \
	gntaccount.h \
	gntblist.h \
	gntconn.h \
	gntconv.h \
	gntdebug.h \
	gntft.h \
	finch.h \
	gntidle.h \
	gntnotify.h \
	gntplugin.h \
	gntpounce.h \
	gntprefs.h \
	gntrequest.h \
	gntstatus.h \
	gntui.h

finchincludedir=$(includedir)/gaim/gnt
finchinclude_HEADERS = \
	$(finch_headers)

finch_DEPENDENCIES = @LIBOBJS@
finch_LDFLAGS = -export-dynamic
finch_LDADD = \
	@LIBOBJS@ \
	$(DBUS_LIBS) \
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(LIBXML_LIBS) \
	$(GNT_LIBS) \
	./libgnt/libgnt.la \
	$(top_builddir)/libpurple/libpurple.la

AM_CPPFLAGS = \
	-DSTANDALONE \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_srcdir) \
	-I$(srcdir)/libgnt/ \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(GNT_CFLAGS)
endif
