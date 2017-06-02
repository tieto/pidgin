SUBDIRS = . wms
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnt.pc

lib_LTLIBRARIES = libgnt.la

noinst_HEADERS = gntinternal.h

libgnt_la_SOURCES = \
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
	gntprogressbar.c \
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
	gntmenu.h \
	gntmenuitem.h \
	gntmenuitemcheck.h \
	gntprogressbar.h \
	gntslider.h \
	gntstyle.h \
	gnttextview.h \
	gnttree.h \
	gntutils.h \
	gntwindow.h \
	gntwm.h \
	gntws.h \
	gnt.h

libgnt_laincludedir=$(includedir)/gnt
libgnt_lainclude_HEADERS = \
	$(libgnt_la_headers)

libgnt_la_DEPENDENCIES = 
libgnt_la_LDFLAGS = -export-dynamic -version-info $(GNT_LT_VERSION_INFO) -no-undefined
libgnt_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GNT_LIBS) \
	$(LIBXML_LIBS) \
	$(INTROSPECTION_LIBS) \
	$(PY_LIBS)

if IS_WIN32

.rc.o: %.rc
	$(AM_V_GEN)$(WINDRES) -i $< -o $@

LIBGNT_WIN32RES = libgnt_winres.o
libgnt_la_DEPENDENCIES += $(LIBGNT_WIN32RES)
libgnt_la_LDFLAGS += -Wl,$(LIBGNT_WIN32RES)

libgnt_la_LIBADD += -lgmodule-2.0

endif

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(INTROSPECTION_CFLAGS) \
	$(PY_CFLAGS)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(prefix)/share/gir-1.0 \
	--warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(prefix)/share/gir-1.0

if HAVE_INTROSPECTION
introspection_sources = $(libgnt_lainclude_HEADERS)

Gnt-$(GNT_MAJOR_VERSION).$(GNT_MINOR_VERSION).gir: $(builddir)/libgnt.la
Gnt_2_9_gir_INCLUDES = GObject-2.0
Gnt_2_9_gir_CFLAGS = \
	$(INCLUDES) \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(INTROSPECTION_CFLAGS) \
	$(PY_CFLAGS)

Gnt_2_9_gir_LIBS = $(builddir)/libgnt.la
Gnt_2_9_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Gnt-$(GNT_MAJOR_VERSION).$(GNT_MINOR_VERSION).gir

girdir = \
	$(prefix)/share/gir-1.0 \
	$(INTROSPECTION_GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = \
	$(prefix)/share/gir-1.0 \
	$(INTROSPECTION_TYPELIBDIR)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif
