plugindir = $(libdir)/pidgin

gevolution_la_LDFLAGS = -module -avoid-version

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

gevolution_la_LIBADD = $(EVOLUTION_ADDRESSBOOK_LIBS) $(GTK_LIBS)

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(EVOLUTION_ADDRESSBOOK_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
