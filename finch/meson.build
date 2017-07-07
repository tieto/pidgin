subdir('libgnt')

libfinch_SOURCES = [
	'gntaccount.c',
	'gntblist.c',
	'gntcertmgr.c',
	'gntconn.c',
	'gntconv.c',
	'gntdebug.c',
	'gntidle.c',
	'gntlog.c',
	'gntmedia.c',
	'gntmenuutil.c',
	'gntnotify.c',
	'gntplugin.c',
	'gntpounce.c',
	'gntprefs.c',
	'gntrequest.c',
	'gntroomlist.c',
	'gntsound.c',
	'gntstatus.c',
	'gntui.c',
	'gntxfer.c',
	package_revision,
	'libfinch.c'
]

libfinch_headers = [
	'gntaccount.h',
	'gntblist.h',
	'gntcertmgr.h',
	'gntconn.h',
	'gntconv.h',
	'gntdebug.h',
	'finch.h',
	'gntidle.h',
	'gntlog.h',
	'gntmedia.h',
	'gntmenuutil.h',
	'gntnotify.h',
	'gntplugin.h',
	'gntpounce.h',
	'gntprefs.h',
	'gntrequest.h',
	'gntroomlist.h',
	'gntsound.h',
	'gntstatus.h',
	'gntui.h',
	'gntxfer.h'
]

finch_SOURCES = [
	'finch.c'
]

if IS_WIN32
#	libfinch_la_LIBADD += \
#		-lwinmm

	finch_winres = configure_file(
	    input : 'finch_winres.rc.in',
	    output : 'finch_winres.rc',
	    configuration : version_conf)
	finch_SOURCES += windows.compile_resources(finch_winres)
	libfinch_winres = configure_file(
	    input : 'libfinch_winres.rc.in',
	    output : 'libfinch_winres.rc',
	    configuration : version_conf)
	libfinch_SOURCES += windows.compile_resources(libfinch_winres)
endif

if enable_consoleui
	install_headers(libfinch_headers, subdir : 'finch')

	libfinch_inc = include_directories('.')
	libfinch = shared_library('finch',
	    libfinch_SOURCES,
	    c_args : '-DSTANDALONE',
	    include_directories : [toplevel_inc],
	    version : PURPLE_LIB_VERSION,
	    dependencies : [libpurple_dep, libgnt_dep, glib],
	    install : true)
	libfinch_dep = declare_dependency(
	    include_directories : [toplevel_inc, libfinch_inc],
	    link_with : libfinch,
	    dependencies : [libpurple_dep, libgnt_dep, glib])

	finch = executable('finch',
	    finch_SOURCES,
	    c_args : '-DSTANDALONE',
	    dependencies : [libpurple_dep, libgnt_dep, libfinch_dep],
	    install : true)

	configure_file(input : 'finch.pc.in',
	               output : 'finch.pc',
	               configuration : pkg_conf,
	               install : true,
	               install_dir : get_option('libdir') + '/pkgconfig')

	if enable_introspection
		introspection_sources = libfinch_headers

		gnome.generate_gir(libfinch,
		    sources : introspection_sources,
		    includes : [libgnt_gir[0], libpurple_gir[0]],
		    namespace : 'Finch',
		    symbol_prefix : 'finch',
		    identifier_prefix : 'Finch',
		    export_packages : 'finch',
		    nsversion : '@0@.@1@'.format(purple_major_version,
		                                 purple_minor_version),
		    install : true)
	endif

	subdir('plugins')
endif  # enable_consoleui
