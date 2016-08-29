ticker_SOURCES = [
	'gtkticker.c',
	'gtkticker.h',
	'ticker.c'
]

if PLUGINS
	ticker = library('ticker', ticker_SOURCES,
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
