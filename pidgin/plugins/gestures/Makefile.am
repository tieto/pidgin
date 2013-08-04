plugindir = $(libdir)/pidgin

gestures_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = gestures.la

gestures_la_SOURCES = \
	gestures.c \
	gstroke.h \
	gstroke-internal.h \
	stroke.c \
	stroke-draw.c

gestures_la_LIBADD = $(GTK_LIBS)

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPLUGIN_CFLAGS)
