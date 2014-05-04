EXTRA_DIST = \
		getopt.c \
		getopt.h \
		getopt1.c \
		finch.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = finch.pc

SUBDIRS = libgnt . plugins

if ENABLE_GNT

# XXX: should this be lib_, or noinst_?
lib_LTLIBRARIES = libfinch.la
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
libfinch_la_LDFLAGS = -export-dynamic -no-undefined \
	-version-info $(PURPLE_LT_VERSION_INFO)
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

if IS_WIN32
libfinch_la_LIBADD += \
	-lwinmm

.rc.o: %.rc
	$(AM_V_GEN)$(WINDRES) -i $< -o $@

FINCH_WIN32RES = finch_winres.o
finch_DEPENDENCIES += $(FINCH_WIN32RES)
finch_LDFLAGS += -Wl,$(FINCH_WIN32RES)

LIBFINCH_WIN32RES = libfinch_winres.o
libfinch_la_DEPENDENCIES += $(LIBFINCH_WIN32RES)
libfinch_la_LDFLAGS += -Wl,$(LIBFINCH_WIN32RES)

endif

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
