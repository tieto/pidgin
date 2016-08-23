plugindir = get_option('libdir') + '/gnt'

irssi = library('irssi', 'irssi.c',
    dependencies : [ncurses, gobject, libgnt_dep],
    name_prefix : '',
    install : true, install_dir : plugindir)

if PURPLE_AVAILABLE
	# These custom wms depend on libpurple
	s = library('s', 's.c',
	    dependencies : [ncurses, gobject, libgnt_dep, libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : plugindir)
endif
