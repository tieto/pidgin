NULLSOURCES = [
	'nullprpl.h',
	'nullprpl.c'
]

# nullprpl isn't built by default; when it is built, it's dynamically linked
if DYNAMIC_NULL
	null_prpl = shared_library('null', NULLSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [gplugin, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
