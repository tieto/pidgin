EXTRA_DIST=genmarshal

SUBDIRS = . wms
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnt.pc

lib_LTLIBRARIES = libgnt.la

libgnt_la_SOURCES = \
	gntmarshal.c \
	gntwidget.c \
	gntbindable.c \
	gntbox.c \
	gntbutton.c \
	gntcheckbox.c \
	gntclipboard.c \
	gntcolors.c \
	gntcombobox.c \
	gntentry.c \
	gntkeys.c \
	gntlabel.c \
	gntline.c \
	gntmenu.c \
	gntmenuitem.c \
	gntmenuitemcheck.c \
	gntstyle.c \
	gnttextview.c \
	gnttree.c \
	gntutils.c \
	gntwindow.c \
	gntwm.c \
	gntmain.c

libgnt_la_headers = \
	gntwidget.h \
	gntbindable.h \
	gntbox.h \
	gntbutton.h \
	gntcheckbox.h \
	gntclipboard.h \
	gntcolors.h \
	gntcombobox.h \
	gntentry.h \
	gntkeys.h \
	gntlabel.h \
	gntline.h \
	gntmarshal.h \
	gntmenu.h \
	gntmenuitem.h \
	gntmenuitemcheck.h \
	gntstyle.h \
	gnttextview.h \
	gnttree.h \
	gntutils.h \
	gntwindow.h \
	gntwm.h \
	gnt.h

CLEANFILES = \
	gntmarshal.h \
	gntmarshal.c

gntmarshal.c: genmarshal gntmarshal.h
	echo "#include \"gntmarshal.h\"" > $@
	cat genmarshal | glib-genmarshal --prefix=gnt_closure_marshal --body >> $@

gntmarshal.h: genmarshal
	cat genmarshal | glib-genmarshal --prefix=gnt_closure_marshal --header > $@

libgnt_laincludedir=$(includedir)/gnt
libgnt_lainclude_HEADERS = \
	$(libgnt_la_headers)

libgnt_la_DEPENDENCIES = 
libgnt_la_LDFLAGS = -export-dynamic
libgnt_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GNT_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(DEBUG_CFLAGS)
