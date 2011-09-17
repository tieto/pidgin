
adiumtemplatedir = $(datadir)/pidgin/webkit
adiumtemplate_DATA = Template.html

adiumdir = $(libdir)/pidgin

adium_la_LDFLAGS = -module -avoid-version

EXTRA_DIST = $(webkittemplate_DATA)

if PLUGINS

adium_LTLIBRARIES = adium.la

adium_la_SOURCES = webkit.c \
	message-style.h \
	message-style.c

endif

adium_la_LIBADD = $(GTK_LIBS) $(WEBKIT_LIBS)

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WEBKIT_CFLAGS)
