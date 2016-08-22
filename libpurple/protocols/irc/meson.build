IRCSOURCES = [
	'cmds.c',
	'dcc_send.c',
	'irc.c',
	'irc.h',
	'msgs.c',
	'parse.c'
]

if STATIC_IRC
	irc_prpl = static_library('irc', IRCSOURCES,
	    c_args : '-DPURPLE_STATIC_PRPL',
	    dependencies : [sasl, libpurple_dep, glib, gio])
elif DYNAMIC_IRC
	irc_prpl = shared_library('irc', IRCSOURCES,
	    dependencies : [sasl, libpurple_dep, glib, gio],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
