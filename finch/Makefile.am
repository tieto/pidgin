EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		finch.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = finch.pc

SUBDIRS = libgnt plugins

if ENABLE_GNT

bin_PROGRAMS = finch

endif

finch_SOURCES = \
	gntaccount.c \
	gntblist.c \
	gntcertmgr.c \
	gntconn.c \
	gntconv.c \
	gntdebug.c \
	finch.c \
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
	gntxfer.c

finch_headers = \
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

finchincludedir=$(includedir)/finch
finchinclude_HEADERS = \
	$(finch_headers)

finch_DEPENDENCIES = @LIBOBJS@
finch_LDFLAGS = -export-dynamic
finch_LDADD = \
	@LIBOBJS@ \
	$(DBUS_LIBS) \
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS) \
	$(LIBXML_LIBS) \
	$(GNT_LIBS) \
	$(GSTREAMER_LIBS) \
	$(INTROSPECTION_LIBS) \
	./libgnt/libgnt.la \
	$(top_builddir)/libpurple/libpurple.la

AM_CPPFLAGS = \
	-DSTANDALONE \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/finch/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_srcdir) \
	-I$(srcdir)/libgnt/ \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(INTROSPECTION_CFLAGS) \
	$(GNT_CFLAGS)

if ENABLE_GNT
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(builddir) --add-include-path=$(top_builddir)/libpurple
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/libpurple

if HAVE_INTROSPECTION
introspection_sources = $(finchinclude_HEADERS)

Finch-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).gir: $(builddir)/finch
Finch_3_0_gir_INCLUDES = GObject-2.0 Purple-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION)
Finch_3_0_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS)
Finch_3_0_gir_PROGRAM = $(builddir)/finch
Finch_3_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Finch-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

endif # ENABLE_GNT
