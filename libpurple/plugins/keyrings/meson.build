if PLUGINS
	internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	if ENABLE_SECRETSERVICE
		secretsservice_plugin = library('secretservice', 'secretservice.c',
		    dependencies : [secretservice, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if ENABLE_GNOMEKEYRING
		gnomekeyring_plugin = library('gnomekeyring', 'gnomekeyring.c',
		    dependencies : [gnome_keyring, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if IS_WIN32
		wincred_plugin = library('wincred', 'wincred.c',
		    dependencies : [libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if ENABLE_KWALLET
		kwallet_moc = moc.process('kwallet.cpp')

		kwallet_plugin = library('kwallet', 'kwallet.cpp', kwallet_moc,
		    dependencies : [kwallet, qt4, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif
endif  # PLUGINS
