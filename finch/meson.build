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

	finch_SOURCES += windows.compile_resources('finch_winres.rc')
	libfinch_SOURCES += windows.compile_resources('libfinch_winres.rc')
endif

if ENABLE_GNT
	install_headers(libfinch_headers, subdir : 'finch')

	libfinch_inc = include_directories('.')
	# TODO: This doesn't quite match autotools' version.
	libfinch = shared_library('finch',
	    libfinch_SOURCES,
	    c_args : '-DSTANDALONE',
	    include_directories : [toplevel_inc],
	    soversion : purple_lt_current,
	    version : PURPLE_LT_VERSION_INFO,
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
	               configuration : conf,
	               install : true,
	               install_dir : get_option('libdir') + '/pkgconfig')

#AM_CPPFLAGS = \
#	-DSTANDALONE \
#	-I$(top_srcdir)/libpurple/ \
#	-I$(top_builddir)/libpurple \
#	-I$(top_srcdir) \
#	-I$(srcdir)/libgnt/ \
#	$(DEBUG_CFLAGS) \
#	$(GLIB_CFLAGS) \
#	$(GPLUGIN_CFLAGS) \
#	$(DBUS_CFLAGS) \
#	$(LIBXML_CFLAGS) \
#	$(GSTREAMER_CFLAGS) \
#	$(INTROSPECTION_CFLAGS) \
#	$(GNT_CFLAGS)

	if enable_introspection
		introspection_sources = libfinch_headers

		gnome.generate_gir(libfinch,
		    sources : introspection_sources,
		    includes : [libgnt_gir[0], libpurple_gir[0]],
		    namespace : 'Finch',
		    symbol_prefix : 'finch_',
		    identifier_prefix : 'Finch',
		    nsversion : '@0@.@1@'.format(purple_major_version,
		                                 purple_minor_version),
		    install : true)
	endif

	subdir('plugins')
endif  # ENABLE_GNT
