if enable_gevolution
	subdir('gevolution')
endif

if enable_dbus
	subdir('musicmessaging')
endif

if enable_cap
	subdir('cap')
endif

if enable_gestures
	subdir('gestures')
endif

subdir('disco')
subdir('ticker')

if IS_WIN32
	subdir('win32/winprefs')
	subdir('win32/transparency')
endif

if PLUGINS
	contact_priority = library('contact_priority', 'contact_priority.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '')

	extplacement = library('extplacement', 'extplacement.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	gtkbuddynote = library('gtkbuddynote', 'gtkbuddynote.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	gtk_signals_test = library('gtk_signals_test', 'gtk-signals-test.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '')

	history = library('history', 'history.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	iconaway = library('iconaway', 'iconaway.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	imgupload = library('imgupload', 'imgupload.c',
	    dependencies : [json, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	markerline = library('markerline', 'markerline.c',
	    dependencies : [webkit, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	notify = library('notify', 'notify.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	relnot = library('relnot', 'relnot.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	screencap = library('screencap', 'screencap.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	sendbutton = library('sendbutton', 'sendbutton.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	spellchk = library('spellchk', 'spellchk.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	webkit_plugin = library('webkit', 'webkit.c',
	    dependencies : [webkit, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	xmppconsole = library('xmppconsole', 'xmppconsole.c',
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)

	if enable_unity
		unity = library('unity', 'unity.c',
		    dependencies : [UNITY, libpurple_dep, libpidgin_dep, glib],
		    name_prefix : '',
		    install : true, install_dir : PIDGIN_PLUGINDIR)
	endif
endif # PLUGINS
