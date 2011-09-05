
webkittemplatedir = $(datadir)/pidgin/webkit
webkittemplate_DATA = Template.html

webkitdir = $(libdir)/pidgin

webkit_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = $(webkittemplate_DATA)

if PLUGINS

webkit_LTLIBRARIES = webkit.la

webkit_la_SOURCES = webkit.c \
	message-style.h \
	message-style.c

endif

webkit_la_LIBADD = $(GTK_LIBS) $(WEBKIT_LIBS)

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WEBKIT_CFLAGS)
