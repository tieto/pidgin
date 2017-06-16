SIMPLESOURCES = [
	'ntlm.c',
	'ntlm.h',
	'simple.c',
	'simple.h',
	'sipmsg.c',
	'sipmsg.h'
]

if STATIC_SIMPLE
	simple_prpl = static_library('simple', SIMPLESOURCES,
	    c_args : '-DPURPLE_STATIC_PRPL',
	    dependencies : [libpurple_dep, glib, gio])
elif DYNAMIC_SIMPLE
	simple_prpl = shared_library('simple', SIMPLESOURCES,
	    dependencies : [libpurple_dep, glib, gio],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
