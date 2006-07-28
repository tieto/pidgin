pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnt.pc

lib_LTLIBRARIES = libgnt.la

libgnt_la_SOURCES = \
	gntwidget.c \
	gntbox.c \
	gntbutton.c \
	gntcheckbox.c \
	gntcolors.c \
	gntcombobox.c \
	gntentry.c \
	gntlabel.c \
	gntline.c \
	gntmarshal.c \
	gntstyle.c \
	gnttextview.c \
	gnttree.c \
	gntmain.c

libgnt_la_headers = \
	gntwidget.h \
	gntbox.h \
	gntbutton.h \
	gntcheckbox.h \
	gntcolors.h \
	gntcombobox.h \
	gntentry.h \
	gntkeys.h \
	gntlabel.h \
	gntline.h \
	gntmarshal.h \
	gntstyle.h \
	gnttextview.h \
	gnttree.h \
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
