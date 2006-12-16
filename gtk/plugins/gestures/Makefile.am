plugindir = $(libdir)/gaim

gestures_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

plugin_LTLIBRARIES = gestures.la

gestures_la_SOURCES = \
	gestures.c \
	gstroke.h \
	gstroke-internal.h \
	stroke.c \
	stroke-draw.c

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/core \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
