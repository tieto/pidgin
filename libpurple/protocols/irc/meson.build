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
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_with : libpurple,
	    dependencies : [sasl, gplugin, glib, gio])
elif DYNAMIC_IRC
	irc_prpl = shared_library('irc', IRCSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [sasl, gplugin, glib, gio],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
