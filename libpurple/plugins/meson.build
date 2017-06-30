subdir('keyrings')

if PLUGINS
	autoaccept = library('autoaccept', 'autoaccept.c',
	    dependencies : [libpurple_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	buddynote = library('buddynote', 'buddynote.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	codeinline = library('codeinline', 'codeinline.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	debug_example = library('debug_example', 'debug_example.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	helloworld = library('helloworld', 'helloworld.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	idle = library('idle', 'idle.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	joinpart = library('joinpart', 'joinpart.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	log_reader = library('log_reader', 'log_reader.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	notify_example = library('notify_example', 'notify_example.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	offlinemsg = library('offlinemsg', 'offlinemsg.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	one_time_password = library('one_time_password', 'one_time_password.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	pluginpref_example = library('pluginpref_example', 'pluginpref_example.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	psychic = library('psychic', 'psychic.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	signals_test = library('signals-test', 'signals-test.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	simple = library('simple-plugin', 'simple.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '')

	statenotify = library('statenotify', 'statenotify.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	if enable_dbus
		dbus_example_bindings_c = custom_target('dbus_example_bindings_c',
		    input : 'dbus-example.c',
		    output : 'dbus-example-bindings.ch',
		    command : [python,
		               meson.source_root() + '/libpurple/dbus-analyze-functions.py',
		               '--export-only', '-o', '@OUTPUT@', '@INPUT@'])

		dbus_example = library('dbus-example', 'dbus-example.c', dbus_example_bindings_c,
		    dependencies : [libpurple_dep, dbus],
		    name_prefix : '')
	endif  # enable_dbus
endif  # PLUGINS
