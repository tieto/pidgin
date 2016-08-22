MXITSOURCES = [
	'actions.c',
	'actions.h',
	'chunk.c',
	'chunk.h',
	'cipher-mxit.c',
	'cipher-mxit.h',
	'client.c',
	'client.h',
	'filexfer.c',
	'filexfer.h',
	'formcmds.c',
	'formcmds.h',
	'login.c',
	'login.h',
	'markup.c',
	'markup.h',
	'multimx.c',
	'multimx.h',
	'mxit.c',
	'mxit.h',
	'profile.c',
	'profile.h',
	'roster.c',
	'roster.h',
	'splashscreen.c',
	'splashscreen.h'
]

if STATIC_MXIT
	mxit_prpl = static_library('mxit', MXITSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_with : libpurple,
	    dependencies : [gplugin, glib])
elif DYNAMIC_MXIT
	mxit_prpl = shared_library('mxit', MXITSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
