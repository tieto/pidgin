SAMETIMESOURCES = [
	'sametime.c',
	'sametime.h'
]

if STATIC_SAMETIME
	sametime_prpl = static_library('sametime', SAMETIMESOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : ['-DG_LOG_DOMAIN="sametime"', '-DPURPLE_STATIC_PRPL'],
	    link_with : libpurple,
	    dependencies : [meanwhile, gplugin, glib])
elif DYNAMIC_SAMETIME
	sametime_prpl = shared_library('sametime', SAMETIMESOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : ['-DG_LOG_DOMAIN="sametime"'],
	    link_with : libpurple,
	    dependencies : [meanwhile, gplugin, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
