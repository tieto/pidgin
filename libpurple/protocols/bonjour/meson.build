BONJOURSOURCES = [
	'bonjour.c',
	'bonjour.h',
	'buddy.c',
	'buddy.h',
	'jabber.c',
	'jabber.h',
	'mdns_common.c',
	'mdns_common.h',
	'mdns_interface.h',
	'mdns_types.h',
	'parser.c',
	'parser.h',
	'bonjour_ft.c',
	'bonjour_ft.h'
]

if IS_WIN32
	BONJOURSOURCES += ['dns_sd_proxy.c']
#	FIXME: libbonjour_la_LIBADD += -lnetapi32
else
	BONJOURSOURCES += ['mdns_avahi.c']
endif

if STATIC_BONJOUR
	bonjour_prpl = static_library('bonjour', BONJOURSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_with : libpurple,
	    dependencies : [libxml, avahi, gplugin, glib])
elif DYNAMIC_BONJOUR
	bonjour_prpl = shared_library('bonjour', BONJOURSOURCES,
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : libpurple,
	    dependencies : [libxml, avahi, gplugin, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
