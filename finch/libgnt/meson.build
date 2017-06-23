libgnt_SOURCES = [
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

if enable_consoleui

install_headers(libgnt_headers, subdir : 'gnt')

if IS_WIN32
	libgnt_SOURCES += windows.compile_resources('libgnt_winres.rc')
endif

libgnt_inc = include_directories('.')
libgnt = library('gnt',
                 libgnt_SOURCES,
                 include_directories : [toplevel_inc],
                 install : true,
                 version : GNT_LIB_VERSION,
                 dependencies : [ncurses, libxml, glib, gobject, gmodule, python_dep])
libgnt_dep = declare_dependency(
    include_directories : [toplevel_inc, libgnt_inc],
    link_with : libgnt,
    dependencies : [ncurses, glib])

configure_file(input : 'gnt.pc.in',
               output : 'gnt.pc',
               configuration : pkg_conf,
               install : true,
               install_dir : get_option('libdir') + '/pkgconfig')

if enable_introspection
	libgnt_gir = gnome.generate_gir(libgnt,
	    sources : libgnt_headers,
	    includes : 'GObject-2.0',
	    namespace : 'Gnt',
	    symbol_prefix : 'gnt',
	    identifier_prefix : 'Gnt',
	    nsversion : '@0@.@1@'.format(gnt_major_version, gnt_minor_version),
	    install : true)
endif

subdir('wms')

endif  # enable_consoleui
