winprefs_SOURCES = [
	'gtkappbar.c',
	'gtkappbar.h',
	'winprefs.c'
]

winprefs = library('winprefs', winprefs_SOURCES,
    dependencies : [libpurple_dep, libpidgin_dep, glib],
    name_prefix : '',
    install : true, install_dir : PIDGIN_PLUGINDIR)
