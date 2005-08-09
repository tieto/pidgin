plugindir = $(libdir)/gaim

if ENABLE_DEBUG
DEBUG_CPPFLAGS = -g -pg
# DEBUG_CPPFLAGS = -D_DEBUG_ -g -pg 
DEBUG_LDFLAGS = -pg
else
DEBUG_CPPFLAGS = -O2
DEBUG_LDFLAGS =
endif

plugin_LTLIBRARIES = crazychat.la
if DISABLE_QT
crazychat_la_LDFLAGS = -module -avoid-version $(DEBUG_LDFLAGS)
crazychat_la_SOURCES = cc_gaim_plugin.c cc_gtk_gl.c cc_gtk_gl.h \
	cc_interface.h crazychat.c crazychat.h util.h \
	cc_network.c cc_network.h filter.c filter.h\
	cc_output.c face.c face.h doggy.c doggy.h glm.h glm.c sharky.h sharky.c models.h models.c

QT_CPPFLAGS = -D_DISABLE_QT_
else
crazychat_la_LDFLAGS = -module -avoid-version $(DEBUG_LDFLAGS) \
	-XCClinker -framework -XCClinker Carbon -XCClinker -framework \
	-XCClinker QuickTime

crazychat_la_SOURCES = cc_features.c cc_gaim_plugin.c cc_gtk_gl.c cc_gtk_gl.h \
	cc_interface.h crazychat.c crazychat.h util.h \
	cc_network.c cc_network.h filter.c filter.h\
	cc_output.c face.c face.h doggy.c doggy.h glm.h glm.c sharky.h sharky.c models.h models.c\
	QTUtilities.c Utilities.c camdata.c camproc.c Utilities.h

QT_CPPFLAGS =
endif

crazychat_la_LIBADD = $(PTHREAD_LIB) $(GTKGLEXT_LIBS)

AM_CFLAGS = $(GTK_CFLAGS) \
	$(GTKGLEXT_CFLAGS) \
	-I$(top_srcdir)/include

AM_CPPFLAGS = -DGAIM_PLUGINS \
	$(DEBUG_CPPFLAGS) \
	$(QT_CPPFLAGS)
