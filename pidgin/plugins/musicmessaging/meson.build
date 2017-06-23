if PLUGINS and enable_dbus
	install_data('music.png',
		install_dir : get_option('datadir') + '/pixmaps/pidgin/buttons')

	music_messaging_bindings = custom_target('music_messaging_bindings.c',
	    command : [python, dbus_analyze_functions, '--export-only', '-o', '@OUTPUT@', '@INPUT@'],
	    input : 'musicmessaging.c',
	    output : 'music-messaging-bindings.ch')

	musicmessaging = library('musicmessaging', 'musicmessaging.c', music_messaging_bindings,
	    dependencies : [dbus, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
