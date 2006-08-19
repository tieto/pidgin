plugindir = $(libdir)/gaim

gevolution_la_LDFLAGS = \
	-module -avoid-version \
	$(EVOLUTION_ADDRESSBOOK_LIBS) \
	$(GTK_LIBS)

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

endif

gevolution_la_LIBADD = 

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(EVOLUTION_ADDRESSBOOK_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
