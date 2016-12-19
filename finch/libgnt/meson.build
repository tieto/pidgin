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

if ENABLE_GNT

install_headers(libgnt_headers, subdir : 'gnt')

libgnt_deps = [ncurses, libxml, glib, gobject] #pythonlib]
if is_win32
	libgnt_deps += [gmodule]
	libgnt_SOURCES += windows.compile_resources('libgnt_winres.rc')
endif

libgnt_inc = include_directories('.')
libgnt = library('gnt',
                 libgnt_SOURCES,
                 include_directories : [toplevel_inc],
                 install : true,
                 soversion : gnt_lt_current,
                 version : GNT_LT_VERSION_INFO,
                 dependencies : libgnt_deps)
libgnt_dep = declare_dependency(
	sources : gntmarshal_h,  # Ensure built before any dependencies.
    include_directories : [toplevel_inc, libgnt_inc],
    link_with : libgnt,
    dependencies : [ncurses, glib])

configure_file(input : 'gnt.pc.in',
               output : 'gnt.pc',
               configuration : conf,
               install : true,
               install_dir : get_option('libdir') + '/pkgconfig')

if enable_introspection
	libgnt_gir = gnome.generate_gir(libgnt,
	    sources : libgnt_headers,
	    includes : 'GObject-2.0',
	    namespace : 'Gnt',
	    symbol_prefix : 'gnt_',
	    identifier_prefix : 'Gnt',
	    nsversion : '@0@.@1@'.format(gnt_major_version, gnt_minor_version),
	    install : true)
endif

subdir('wms')

endif  # ENABLE_GNT
