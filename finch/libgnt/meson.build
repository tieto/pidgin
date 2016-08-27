marshallers = gnome.genmarshal('gntmarshal',
    sources : 'gntmarshal',
    prefix : 'gnt_closure_marshal',
    install_header : true,
    install_dir : get_option('includedir') + '/gnt')
gntmarshal_c = marshallers[0]
gntmarshal_h = marshallers[1]

libgnt_SOURCES = [
	gntmarshal_c,
	gntmarshal_h,
	'gntwidget.c',
	'gntbindable.c',
	'gntbox.c',
	'gntbutton.c',
	'gntcheckbox.c',
	'gntclipboard.c',
	'gntcolors.c',
	'gntcombobox.c',
	'gntentry.c',
	'gntfilesel.c',
	'gntkeys.c',
	'gntlabel.c',
	'gntline.c',
	'gntmenu.c',
	'gntmenuitem.c',
	'gntmenuitemcheck.c',
	'gntprogressbar.c',
	'gntslider.c',
	'gntstyle.c',
	'gnttextview.c',
	'gnttree.c',
	'gntutils.c',
	'gntwindow.c',
	'gntwm.c',
	'gntws.c',
	'gntmain.c'
]

libgnt_headers = [
	'gntwidget.h',
	'gntbindable.h',
	'gntbox.h',
	'gntbutton.h',
	'gntcheckbox.h',
	'gntclipboard.h',
	'gntcolors.h',
	'gntcombobox.h',
	'gntentry.h',
	'gntfilesel.h',
	'gntkeys.h',
	'gntlabel.h',
	'gntline.h',
	'gntmenu.h',
	'gntmenuitem.h',
	'gntmenuitemcheck.h',
	'gntprogressbar.h',
	'gntslider.h',
	'gntstyle.h',
	'gnttextview.h',
	'gnttree.h',
	'gntutils.h',
	'gntwindow.h',
	'gntwm.h',
	'gntws.h',
	'gnt.h'
]

install_headers(libgnt_headers, subdir : 'gnt')

libgnt_inc = include_directories('.')
libgnt = library('gnt',
                 libgnt_SOURCES,
                 include_directories : [toplevel_inc],
                 install : true,
                 soversion : gnt_lt_current,
                 version : GNT_LT_VERSION_INFO,
                 dependencies : [ncurses, libxml, glib, gmodule, gobject])#pythonlib])
libgnt_dep = declare_dependency(
    include_directories : [toplevel_inc, libgnt_inc],
    link_with : libgnt,
    dependencies : [ncurses, glib])

configure_file(input : 'gnt.pc.in',
               output : 'gnt.pc',
               configuration : conf,
               install : true,
               install_dir : get_option('libdir') + '/pkgconfig')

if IS_WIN32
	# TODO: Do something with this:
	LIBGNT_WIN32RES = WINDRES.process('libgnt_winres.rc')
endif

#-include $(INTROSPECTION_MAKEFILE)
#INTROSPECTION_GIRS =
#INTROSPECTION_SCANNER_ARGS = \
#	--add-include-path=$(prefix)/share/gir-1.0 \
#	--warn-all
#INTROSPECTION_COMPILER_ARGS = --includedir=$(prefix)/share/gir-1.0

if enable_introspection
#introspection_sources = $(libgnt_lainclude_HEADERS)

#Gnt-$(GNT_MAJOR_VERSION).$(GNT_MINOR_VERSION).gir: $(builddir)/libgnt.la
#Gnt_2_9_gir_INCLUDES = GObject-2.0
#Gnt_2_9_gir_CFLAGS = \
#	$(INCLUDES) \
#	$(GLIB_CFLAGS) \
#	$(GNT_CFLAGS) \
#	$(LIBXML_CFLAGS) \
#	$(INTROSPECTION_CFLAGS) \
#	$(PY_CFLAGS)

#Gnt_2_9_gir_LIBS = $(builddir)/libgnt.la
#Gnt_2_9_gir_FILES = $(introspection_sources)
#INTROSPECTION_GIRS += Gnt-$(GNT_MAJOR_VERSION).$(GNT_MINOR_VERSION).gir

#girdir = \
#	$(prefix)/share/gir-1.0 \
#	$(INTROSPECTION_GIRDIR)
#gir_DATA = $(INTROSPECTION_GIRS)

#typelibdir = \
#	$(prefix)/share/gir-1.0 \
#	$(INTROSPECTION_TYPELIBDIR)
#typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)
endif

subdir('wms')
