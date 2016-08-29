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
	    dependencies : [libpurple_dep, libgnt_dep, glib])
	libfinch_dep = declare_dependency(
	    include_directories : [toplevel_inc, libfinch_inc],
	    link_with : libfinch,
	    dependencies : [libpurple_dep, libgnt_dep, glib])

	finch = executable('finch',
	    finch_SOURCES,
	    c_args : '-DSTANDALONE',
	    dependencies : [libpurple_dep, libgnt_dep, libfinch_dep])

	configure_file(input : 'finch.pc.in',
	               output : 'finch.pc',
	               configuration : conf,
	               install : true,
	               install_dir : get_option('libdir') + '/pkgconfig')
endif

if IS_WIN32
#	libfinch_la_LIBADD += \
#		-lwinmm

	# TODO: Use this for something:
	FINCH_WIN32RES = WINDRES.process('finch_winres.rc')
	LIBFINCH_WIN32RES = WINDRES.process('libfinch_winres.rc')
endif

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

if ENABLE_GNT
#	-include $(INTROSPECTION_MAKEFILE)
#	INTROSPECTION_GIRS =
#	INTROSPECTION_SCANNER_ARGS = \
#		--warn-all \
#		--add-include-path=$(prefix)/share/gir-1.0 \
#		--add-include-path=$(builddir)/libgnt \
#		--add-include-path=$(top_builddir)/libpurple

#	INTROSPECTION_COMPILER_ARGS = \
#		--includedir=$(prefix)/share/gir-1.0 \
#		--includedir=$(builddir)/libgnt \
#		--includedir=$(top_builddir)/libpurple

	if enable_introspection
#		introspection_sources = $(libfinchinclude_HEADERS)

#		Finch-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).gir: $(builddir)/libfinch.la
#		Finch_3_0_gir_INCLUDES = \
#			Gnt-$(GNT_MAJOR_VERSION).$(GNT_MINOR_VERSION) \
#			Purple-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION)

#		Finch_3_0_gir_CFLAGS = \
#			$(INCLUDES) \
#			-DSTANDALONE \
#			-DDATADIR=\"$(datadir)\" \
#			-DLIBDIR=\"$(libdir)/finch/\" \
#			-DLOCALEDIR=\"$(datadir)/locale\" \
#			-DSYSCONFDIR=\"$(sysconfdir)\" \
#			-I$(top_srcdir)/libpurple/ \
#			-I$(top_builddir)/libpurple/ \
#			-I$(top_srcdir) \
#			-I$(srcdir)/libgnt/ \
#			$(GLIB_CFLAGS) \
#			$(GPLUGIN_CFLAGS) \
#			$(DBUS_CFLAGS) \
#			$(LIBXML_CFLAGS) \
#			$(GSTREAMER_CFLAGS) \
#			$(INTROSPECTION_CFLAGS) \
#			$(GNT_CFLAGS)

#		Finch_3_0_gir_LIBS = $(builddir)/libfinch.la
#		Finch_3_0_gir_FILES = $(introspection_sources)
#		INTROSPECTION_GIRS += Finch-$(PURPLE_MAJOR_VERSION).$(PURPLE_MINOR_VERSION).gir

#		girdir = \
#			 $(prefix)/share/gir-1.0 \
#			 $(INTROSPECTION_GIRDIR)
#		gir_DATA = $(INTROSPECTION_GIRS)

#		typelibdir = \
#			$(prefix)/share/gir-1.0 \
#			$(INTROSPECTION_TYPELIBDIR)
#		typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)
	endif
endif  # ENABLE_GNT

subdir('plugins')
