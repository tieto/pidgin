SAMETIMESOURCES = [
	'sametime.c',
	'sametime.h'
]

if STATIC_SAMETIME
	sametime_prpl = static_library('sametime', SAMETIMESOURCES,
	    c_args : ['-DG_LOG_DOMAIN="sametime"', '-DPURPLE_STATIC_PRPL'],
	    dependencies : [meanwhile, libpurple_dep, glib])
elif DYNAMIC_SAMETIME
	sametime_prpl = shared_library('sametime', SAMETIMESOURCES,
	    c_args : ['-DG_LOG_DOMAIN="sametime"'],
	    dependencies : [meanwhile, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
