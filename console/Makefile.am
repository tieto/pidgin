if ENABLE_GNT

SUBDIRS = libgnt plugins

bin_PROGRAMS = gaim-text

gaim_text_SOURCES = \
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

gaim_text_headers = \
	gntaccount.h \
	gntblist.h \
	gntconn.h \
	gntconv.h \
	gntdebug.h \
	gntgaim.h \
	gntnotify.h \
	gntplugin.h \
	gntprefs.h \
	gntrequest.h \
	gntstatus.h \
	gntui.h

gaim_textincludedir=$(includedir)/gaim/gnt
gaim_textinclude_HEADERS = \
	$(gaim_text_headers)

gaim_text_DEPENDENCIES = @LIBOBJS@
gaim_text_LDFLAGS = -export-dynamic
gaim_text_LDADD = \
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
