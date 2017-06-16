libpidgin_SOURCES = [
	'pidginstock.c',
	'gtkaccount.c',
	'gtkblist.c',
	'gtkblist-theme.c',
	'gtkblist-theme-loader.c',
	'gtkcellrendererexpander.c',
	'gtkcertmgr.c',
	'gtkconn.c',
	'gtkconv.c',
	'gtkconv-theme.c',
	'gtkconv-theme-loader.c',
	'gtkdebug.c',
	'gtkdialogs.c',
	'gtkdnd-hints.c',
	'gtkdocklet.c',
	'gtkeventloop.c',
	'gtkicon-theme.c',
	'gtkicon-theme-loader.c',
	'gtkidle.c',
	'gtklog.c',
	'gtkmedia.c',
	'gtkmenutray.c',
	'gtknotify.c',
	'gtkplugin.c',
	'gtkpluginpref.c',
	'gtkpounce.c',
	'gtkprefs.c',
	'gtkprivacy.c',
	'gtkrequest.c',
	'gtkroomlist.c',
	'gtksavedstatuses.c',
	'gtkscrollbook.c',
	'gtksession.c',
	'gtksmiley-manager.c',
	'gtksmiley-theme.c',
	'gtksound.c',
	'gtkstatus-icon-theme.c',
	'gtkstatusbox.c',
	'gtkutils.c',
	'gtkwebview.c',
	'gtkwebviewtoolbar.c',
	'gtkwhiteboard.c',
	'gtkxfer.c',
	'libpidgin.c',
	'minidialog.c',
	'pidgintooltip.c'
]

libpidgin_headers = [
	'gtkaccount.h',
	'gtkblist.h',
	'gtkblist-theme.h',
	'gtkblist-theme-loader.h',
	'gtkcellrendererexpander.h',
	'gtkcertmgr.h',
	'gtkconn.h',
	'gtkconv.h',
	'gtkconvwin.h',
	'gtkconv-theme.h',
	'gtkconv-theme-loader.h',
	'gtkdebug.h',
	'gtkdialogs.h',
	'gtkdnd-hints.h',
	'gtkdocklet.h',
	'gtkeventloop.h',
	'gtkicon-theme.h',
	'gtkicon-theme-loader.h',
	'gtkidle.h',
	'gtklog.h',
	'gtkmedia.h',
	'gtkmenutray.h',
	'gtknickcolors.h',
	'gtknotify.h',
	'gtkplugin.h',
	'gtkpluginpref.h',
	'gtkprefs.h',
	'gtkprivacy.h',
	'gtkpounce.h',
	'gtkrequest.h',
	'gtkroomlist.h',
	'gtksavedstatuses.h',
	'gtkscrollbook.h',
	'gtksession.h',
	'gtksmiley-manager.h',
	'gtksmiley-theme.h',
	'gtksound.h',
	'gtkstatus-icon-theme.h',
	'gtkstatusbox.h',
	'pidginstock.h',
	'gtkutils.h',
	'gtkwebview.h',
	'gtkwebviewtoolbar.h',
	'gtkwhiteboard.h',
	'gtkxfer.h',
	'minidialog.h',
	'pidgintooltip.h',
	'pidgin.h'
]

pidgin_SOURCES = [
	'pidgin.c'
]

gtkdebug_html = gnome.compile_resources('gtkdebug.gresource', 'gtkdebug.gresource.xml',
  c_name : 'gtkdebug')
libpidgin_SOURCES += gtkdebug_html

if IS_WIN32
	libpidgin_SOURCES += [
		'win32/gtkwin32dep.c',
		'win32/untar.c'
	]

	# Files that looks like obsolete (were used in Pidgin2):
	#	win32/gtkdocklet-win32.c
	#	win32/MinimizeToTray.c
	#	win32/MinimizeToTray.h

	pidgin_SOURCES += [
		'win32/winpidgin.c',
		windows.compile_resources('win32/pidgin_exe_rc.rc')
	]

	libpidgin_headers += [
		'win32/gtkwin32dep.h',
		'win32/resource.h',
		'win32/untar.h'
	]
endif

if ENABLE_GTK
	if IS_WIN32
		libpidgin_SOURCES += windows.compile_resources('win32/pidgin_dll_rc.rc')
	endif

	install_headers(libpidgin_headers, subdir : 'pidgin')

	libpidgin_inc = include_directories('.')
	# TODO: This doesn't quite match autotools' version.
	libpidgin = shared_library('pidgin',
	    libpidgin_SOURCES,
	    include_directories : [toplevel_inc],
	    soversion : purple_lt_current,
	    version : PURPLE_LT_VERSION_INFO,
	    dependencies : [gtk, gstreamer_video, webkit, enchant, dbus, x11, sm, xss, GCR, libpurple_dep, glib, math],
	    install : true)
	libpidgin_dep = declare_dependency(
	    include_directories : [toplevel_inc, libpidgin_inc],
	    link_with : libpidgin,
	    dependencies : [webkit, gtk, glib, math])

	pidgin = executable('pidgin',
	    pidgin_SOURCES,
	    link_args : '-export-dynamic',
	    dependencies : [libpurple_dep, libpidgin_dep],
	    install : true)

	if IS_WIN32
#		libpidgin_la_LIBADD += -lwinmm
#		pidgin_LDFLAGS += -mwindows
	endif

	configure_file(input : 'data/pidgin-3.pc.in',
	               output : 'pidgin-3.pc',
	               configuration : conf,
	               install : true,
	               install_dir : get_option('libdir') + '/pkgconfig')

	if INSTALL_I18N
		DESKTOP_FILE = 'pidgin.desktop'
		desktop_file_in = configure_file(
		    input : 'data/' + DESKTOP_FILE + '.in.in',
		    output : DESKTOP_FILE + '.in',
		    configuration : conf)
		desktop_file = custom_target(DESKTOP_FILE,
		    input : desktop_file_in,
		    output : DESKTOP_FILE,
		    command : [intltool_merge, '--desktop-style', '-u',
		               '-c', '@BUILD_DIR@/po/.intltool-merge-cache',
		               meson.source_root() + '/po', '@INPUT@', '@OUTPUT@'],
		    install : true,
		    install_dir : get_option('datadir') + '/applications')

		appdata = custom_target('pidgin.appdata.xml',
		    input : 'data/pidgin.appdata.xml.in',
		    output : 'pidgin.appdata.xml',
			command : [intltool_merge, '--xml-style', '-u',
		               '-c', '@BUILD_DIR@/po/.intltool-merge-cache',
		               meson.source_root() + '/po', '@INPUT@', '@OUTPUT@'],
		    install : true,
		    install_dir : get_option('datadir') + '/appdata')
	endif  # INSTALL_I18N

	if enable_introspection
		introspection_sources = libpidgin_headers

		gnome.generate_gir(libpidgin,
		    sources : introspection_sources,
		    includes : ['GObject-2.0', 'Gtk-3.0', libpurple_gir[0]],
		    namespace : 'Pidgin',
		    symbol_prefix : 'pidgin_',
		    identifier_prefix : 'Pidgin',
		    nsversion : '@0@.@1@'.format(purple_major_version,
		                                 purple_minor_version),
		    install : true)
	endif

	subdir('pixmaps')
	subdir('plugins')
	subdir('themes')
endif  # ENABLE_GTK
