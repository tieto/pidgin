plugindir = $(libdir)/gaim

ticker_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = ticker.la

ticker_la_SOURCES = \
	gtkticker.c \
	gtkticker.h \
	ticker.c

endif

CFLAGS += $(DEBUG_CFLAGS)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-DVERSION=\"$(VERSION)\"
