if PLUGINS
	internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	if ENABLE_SECRETSERVICE
		secretsservice_plugin = library('secretservice', 'secretservice.c',
		    include_directories : [toplevel_inc, libpurple_inc],
		    link_with : libpurple,
		    dependencies : [secretservice, gplugin, glib],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if ENABLE_GNOMEKEYRING
		gnomekeyring_plugin = library('gnomekeyring', 'gnomekeyring.c',
		    include_directories : [toplevel_inc, libpurple_inc],
		    link_with : libpurple,
		    dependencies : [gnome_keyring, gplugin, glib],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if IS_WIN32
		wincred_plugin = library('wincred', 'wincred.c',
		    include_directories : [toplevel_inc, libpurple_inc],
		    link_with : libpurple,
		    dependencies : [gplugin, glib],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if ENABLE_KWALLET
		kwallet_moc = moc.process('kwallet.cpp')

		kwallet_plugin = library('kwallet', 'kwallet.cpp', kwallet_moc,
		    include_directories : [toplevel_inc, libpurple_inc],
		    link_with : libpurple,
		    dependencies : [kwallet, qt4, gplugin, glib],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif
endif  # PLUGINS
