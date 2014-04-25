plugindir = @PURPLE_PLUGINDIR@

plugin_LTLIBRARIES = mono.la

mono_la_SOURCES	= \
	mono.c \
	mono-glue.h \
	mono-helper.c \
	mono-helper.h \
	debug-glue.c \
	signal-glue.c \
	blist-glue.c \
	status-glue.c 

mono_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

mono_la_LIBADD = $(GPLUGIN_LIBS) $(MONO_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(MONO_CFLAGS)
