SILCSOURCES = [
	'buddy.c',
	'chat.c',
	'ft.c',
	'ops.c',
	'pk.c',
	'silc.c',
	'silcpurple.h',
	'util.c',
	'wb.c',
	'wb.h'
]

if STATIC_SILC
	silc_prpl = static_library('silcpurple', SILCSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_with : libpurple,
	    dependencies : [silc, gplugin, glib])
elif DYNAMIC_SILC
	silc_prpl = shared_library('silcpurple', SILCSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [silc, gplugin, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
