pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnt.pc

lib_LTLIBRARIES = libgnt.la

libgnt_la_SOURCES = \
	gntwidget.c \
	gntbox.c \
	gntbutton.c \
	gntcolors.c \
	gntentry.c \
	gntlabel.c \
	gnttextview.c \
	gnttree.c \
	gntutils.c \
	gntmain.c

libgnt_la_headers = \
	gntwidget.h \
	gntbox.h \
	gntbutton.h \
	gntcolors.h \
	gntentry.h \
	gntkeys.h \
	gntlabel.h \
	gnttextview.h \
	gnttree.h \
	gntutils.h \
	gnt.h

libgnt_laincludedir=$(includedir)/gnt
libgnt_lainclude_HEADERS = \
	$(libgnt_la_headers)

libgnt_la_DEPENDENCIES = @LIBOBJS@ $(STATIC_LINK_LIBS) $(MS_LIBS)
libgnt_la_LDFLAGS = -export-dynamic
libgnt_la_LIBADD = \
	$(GLIB_LIBS) \
	$(STATIC_LINK_LIBS) \
	-lncursesw -lpanelw

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-Wall
