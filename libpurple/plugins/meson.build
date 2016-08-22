subdir('keyrings')

if PLUGINS
	autoaccept = library('autoaccept', 'autoaccept.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	buddynote = library('buddynote', 'buddynote.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	caesarcipher = library('caesarcipher', 'caesarcipher.c', 'caesarcipher.h',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

#	caesarcipher_consumer = library('caesarcipher_consumer', 'caesarcipher_consumer.c',
#	    include_directories : [toplevel_inc, libpurple_inc],
#	    link_with : libpurple,
#	    dependencies : [gplugin, glib],
#	    name_prefix : '')

	codeinline = library('codeinline', 'codeinline.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	debug_example = library('debug_example', 'debug_example.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	helloworld = library('helloworld', 'helloworld.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	idle = library('idle', 'idle.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	joinpart = library('joinpart', 'joinpart.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	log_reader = library('log_reader', 'log_reader.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	notify_example = library('notify_example', 'notify_example.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	offlinemsg = library('offlinemsg', 'offlinemsg.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	one_time_password = library('one_time_password', 'one_time_password.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	pluginpref_example = library('pluginpref_example', 'pluginpref_example.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	psychic = library('psychic', 'psychic.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	signals_test = library('signals-test', 'signals-test.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	simple = library('simple', 'simple.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '')

	statenotify = library('statenotify', 'statenotify.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	if ENABLE_DBUS
		dbus_example_bindings_c = custom_target('dbus_example_bindings_c',
		    input : 'dbus-example.c',
		    output : 'dbus-example-bindings.c',
		    command : [python,
		               meson.source_root() + '/libpurple/dbus-analyze-functions.py',
		               '--export-only', '-o', '@OUTPUT@', '@INPUT@'])

		dbus_example = library('dbus-example', 'dbus-example.c',
		    include_directories : [toplevel_inc, libpurple_inc],
		    link_with : libpurple,
		    dependencies : [dbus, gplugin, glib],
		    name_prefix : '')
	endif  # ENABLE_DBUS
endif  # PLUGINS
