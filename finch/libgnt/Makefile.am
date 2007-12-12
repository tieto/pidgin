EXTRA_DIST=genmarshal pygnt

SUBDIRS = . wms
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnt.pc

lib_LTLIBRARIES = libgnt.la

BUILT_SOURCES = gntmarshal.h

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
	gntfilesel.c \
	gntkeys.c \
	gntlabel.c \
	gntline.c \
	gntmenu.c \
	gntmenuitem.c \
	gntmenuitemcheck.c \
	gntslider.c \
	gntstyle.c \
	gnttextview.c \
	gnttree.c \
	gntutils.c \
	gntwindow.c \
	gntwm.c \
	gntws.c \
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
	gntfilesel.h \
	gntkeys.h \
	gntlabel.h \
	gntline.h \
	gntmarshal.h \
	gntmenu.h \
	gntmenuitem.h \
	gntmenuitemcheck.h \
	gntslider.h \
	gntstyle.h \
	gnttextview.h \
	gnttree.h \
	gntutils.h \
	gntwindow.h \
	gntwm.h \
	gntws.h \
	gnt.h

CLEANFILES = \
	gntmarshal.h \
	gntmarshal.c

gntmarshal.c: $(srcdir)/genmarshal gntmarshal.h
	echo "#include \"gntmarshal.h\"" > $@
	glib-genmarshal --prefix=gnt_closure_marshal --body $(srcdir)/genmarshal >> $@

gntmarshal.h: $(srcdir)/genmarshal
	glib-genmarshal --prefix=gnt_closure_marshal --header $(srcdir)/genmarshal > $@

libgnt_laincludedir=$(includedir)/gnt
libgnt_lainclude_HEADERS = \
	$(libgnt_la_headers)

libgnt_la_DEPENDENCIES = 
libgnt_la_LDFLAGS = -export-dynamic -version_info $(GNT_LT_VERSION_INFO) -no-undefined
libgnt_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GNT_LIBS) \
	$(LIBXML_LIBS) \
	$(PY_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(PY_CFLAGS)
