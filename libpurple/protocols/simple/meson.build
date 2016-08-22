SIMPLESOURCES = [
	'simple.c',
	'simple.h',
	'sipmsg.c',
	'sipmsg.h'
]

if STATIC_SIMPLE
	simple_prpl = static_library('simple', SIMPLESOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_with : libpurple,
	    dependencies : [gplugin, glib, gio])
elif DYNAMIC_SIMPLE
	simple_prpl = shared_library('simple', SIMPLESOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib, gio],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
