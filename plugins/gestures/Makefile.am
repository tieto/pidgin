plugindir = $(libdir)/gaim

gestures_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = gestures.la

gestures_la_SOURCES = \
	gestures.c \
	gstroke.h \
	gstroke-internal.h \
	stroke.c \
	stroke-draw.c

endif

INCLUDES = \
	-I$(top_srcdir)/src \
	-DVERSION=\"$(VERSION)\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)
