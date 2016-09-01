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

# FIXME: Need to write a portable way to do this.

gtkdebug_html_body1 = custom_target('gtkdebug.html.body1.h',
    input : 'gtkdebug.html',
    output : 'gtkdebug.html.body1.h',
	command : [sedpath,
               '-e', 's/^[ 	]\+//g',
               '-e', 's/[ 	]\+/ /g',
               '@INPUT@'],
    capture : true)
gtkdebug_html_body2 = custom_target('gtkdebug.html.body2.h',
    input : gtkdebug_html_body1,
    output : 'gtkdebug.html.body2.h',
	command : [xxdpath, '-i', '@INPUT@', '@OUTPUT@'])
gtkdebug_html_h = custom_target('gtkdebug.html.h',
    input : gtkdebug_html_body2,
    output : 'gtkdebug.html.h',
	command : [sedpath,
               '-e', 's/\(0x[0-9a-f][0-9a-f]\)$/\1, 0x00/',
               '-e', 's/pidgin_gtkdebug_html_body1_h/gtkdebug_html/g',
               '@INPUT@'],
    capture : true)

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
		'win32/pidgin_exe_rc.rc'
	]

	libpidgin_headers += [
		'win32/gtkwin32dep.h',
		'win32/resource.h',
		'win32/untar.h'
	]
endif

if ENABLE_GTK
	if IS_WIN32
		# TODO: Do something with this.
		LIBPIDGIN_WIN32RES = WINDRES.process('win32/pidgin_dll_rc.rc')
	endif

	install_headers(libpidgin_headers, subdir : 'pidgin')

	libpidgin_inc = include_directories('.')
	# TODO: This doesn't quite match autotools' version.
	libpidgin = shared_library('pidgin',
	    libpidgin_SOURCES,
	    include_directories : [toplevel_inc],
	    soversion : purple_lt_current,
	    dependencies : [gtk, gstreamer_video, webkit, enchant, dbus, x11, sm, xss, libpurple_dep, glib, math])
	libpidgin_dep = declare_dependency(
	    include_directories : [toplevel_inc, libpidgin_inc],
	    link_with : libpidgin,
	    dependencies : [webkit, gtk, glib, math])
#	libpidgin_la_LDFLAGS = -export-dynamic \
#	libpidgin_la_LIBADD = \
#		@LIBOBJS@ \
#		$(ENCHANT_LIBS) \
#		$(GLIB_LIBS) \
#		$(GPLUGIN_LIBS) \
#		$(GCR_LIBS) \
#		$(DBUS_LIBS) \
#		$(GSTREAMER_LIBS) \
#		$(GSTVIDEO_LIBS) \
#		$(GSTINTERFACES_LIBS) \
#		$(XSS_LIBS) \
#		$(SM_LIBS) \
#		$(INTLLIBS) \
#		$(LIBXML_LIBS) \
#		$(WEBKIT_LIBS) \
#		$(GTK_LIBS) \
#		$(X11_LIBS) \
#		$(INTROSPECTION_LIBS) \

	pidgin = executable('pidgin',
	    pidgin_SOURCES,
	    link_args : '-export-dynamic',
	    dependencies : [libpurple_dep, libpidgin_dep])

	if IS_WIN32
#		libpidgin_la_LIBADD += -lwinmm
#		pidgin_LDFLAGS += -mwindows
	endif

#	AM_CPPFLAGS = \
#		$(ENCHANT_CFLAGS) \
#		$(GLIB_CFLAGS) \
#		$(GPLUGIN_CFLAGS) \
#		$(GCR_CFLAGS) \
#		$(GSTREAMER_CFLAGS) \
#		$(GSTVIDEO_CFLAGS) \
#		$(GSTINTERFACES_CFLAGS) \
#		$(DEBUG_CFLAGS) \
#		$(GTK_CFLAGS) \
#		$(X11_CFLAGS) \
#		$(DBUS_CFLAGS) \
#		$(LIBXML_CFLAGS) \
#		$(WEBKIT_CFLAGS) \
#		$(INTROSPECTION_CFLAGS)

	configure_file(input : 'data/pidgin-3.pc.in',
	               output : 'pidgin-3.pc',
	               configuration : conf,
	               install : true,
	               install_dir : get_option('libdir') + '/pkgconfig')

	if INSTALL_I18N
		DESKTOP_FILE = 'data/pidgin.desktop'

#		appsdir = $(datadir)/applications
#		apps_in_files = data/pidgin.desktop.in
#		apps_DATA = $(apps_in_files:.desktop.in=.desktop)

		# silenced INTLTOOL_DESKTOP_RULE
#		%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po)
#			$(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@ > /dev/null

#		appdatadir = $(datarootdir)/appdata
#		appdata_DATA = $(appdata_in_files:.xml.in=.xml)
#		appdata_in_files = data/pidgin.appdata.xml.in

#		@INTLTOOL_XML_RULE@

	endif # INSTALL_I18N

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
