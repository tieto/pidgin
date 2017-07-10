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
	    c_args : '-DPURPLE_STATIC_PRPL',
	    dependencies : [silc, libpurple_dep, glib])
elif DYNAMIC_SILC
	silc_prpl = shared_library('silcpurple', SILCSOURCES,
	    dependencies : [silc, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
