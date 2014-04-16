plugindir = $(libdir)/pidgin

gevolution_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if PLUGINS

plugin_LTLIBRARIES = gevolution.la

gevolution_la_SOURCES = \
	add_buddy_dialog.c \
	assoc-buddy.c \
	gevolution.c \
	gevolution.h \
	gevo-util.c \
	new_person_dialog.c \
	eds-utils.c

gevolution_la_LIBADD = @PIDGIN_LIBS@ $(EVOLUTION_ADDRESSBOOK_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(EVOLUTION_ADDRESSBOOK_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
