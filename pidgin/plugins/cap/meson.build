if PLUGINS
	cap = library('cap', 'cap.c', 'cap.h', 'cap_statistics.h',
	    dependencies : [sqlite, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
