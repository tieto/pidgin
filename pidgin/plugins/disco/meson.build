xmppdisco_SOURCES = [
	'gtkdisco.c',
	'gtkdisco.h',
	'xmppdisco.c',
	'xmppdisco.h'
]

if PLUGINS
	xmppdisco = library('xmppdisco', xmppdisco_SOURCES,
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
