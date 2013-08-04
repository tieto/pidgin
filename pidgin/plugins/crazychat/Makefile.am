plugindir = $(libdir)/pidgin

if ENABLE_DEBUG
DEBUG_CPPFLAGS = -g -pg
# DEBUG_CPPFLAGS = -D_DEBUG_ -g -pg
DEBUG_LFLAGS = -pg
else
DEBUG_CPPFLAGS = -O2
DEBUG_LFLAGS =
endif

plugin_LTLIBRARIES = crazychat.la

if DISABLE_MODELS
MD_CPPFLAGS = -DDISABLE_MODELS
else
MD_CPPFLAGS =
endif

crazychat_la_LDFLAGS = -module -avoid-version $(GTK_LIBS) $(DEBUG_LFLAGS)
crazychat_la_SOURCES = cc_pidgin_plugin.c cc_gtk_gl.c cc_gtk_gl.h \
	cc_interface.h crazychat.c crazychat.h util.h\
	cc_network.c cc_network.h filter.c filter.h\
	cc_output.c face.c face.h doggy.c doggy.h glm.h glm.c sharky.h sharky.c models.h models.c

QT_CPPFLAGS = -D_DISABLE_QT_

crazychat_la_LIBADD = $(GTKGLEXT_LIBS)

#AM_CFLAGS = $(GTK_CFLAGS) \
#$(GTKGLEXT_CFLAGS) \
#-I$(top_srcdir)/include

AM_CPPFLAGS = -DPURPLE_PLUGINS \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(GTK_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(GTKGLEXT_CFLAGS) \
	$(DEBUG_CPPFLAGS) \
	$(DEBUG_CFLAGS) \
	$(QT_CPPFLAGS) \
	$(MD_CPPFLAGS)
