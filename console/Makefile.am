if ENABLE_GNT

SUBDIRS = libgnt plugins

bin_PROGRAMS = gntgaim

gntgaim_SOURCES = \
	gntaccount.c \
	gntblist.c \
	gntconn.c \
	gntconv.c \
	gntdebug.c \
	gntgaim.c \
	gntnotify.c \
	gntplugin.c \
	gntprefs.c \
	gntrequest.c \
	gntstatus.c \
	gntui.c

gntgaim_headers = \
	gntaccount.h \
	gntblist.h \
	gntconn.h \
	gntconv.h \
	gntdebug.h \
	gntnotify.h \
	gntplugin.c \
	gntprefs.h \
	gntrequest.h \
	gntstatus.h \
	gntui.h

gntgaimincludedir=$(includedir)/gaim/gnt
gntgaiminclude_HEADERS = \
	$(gntgaim_headers)

gntgaim_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS)
gntgaim_LDFLAGS = -export-dynamic
gntgaim_LDADD = \
	@LIBOBJS@ \
	$(DBUS_LIBS) \
	$(STATIC_LINK_LIBS) \
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(LIBXML_LIBS) \
	$(GNT_LIBS) \
	-L./libgnt/ -lgnt \
	-L$(top_builddir)/libgaim -lgaim

AM_CPPFLAGS = \
	-DSTANDALONE \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/libgaim/ \
	-I$(top_srcdir) \
	-I$(srcdir)/libgnt/ \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(GNT_CFLAGS)
endif
