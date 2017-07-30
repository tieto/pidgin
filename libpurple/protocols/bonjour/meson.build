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
	BONJOURSOURCES += ['dns_sd_proxy.c', 'mdns_dns_sd.c']
	bonjour_link_args = ['-lnetapi32']
else
	BONJOURSOURCES += ['mdns_avahi.c']
	bonjour_link_args = []
endif

if STATIC_BONJOUR
	bonjour_prpl = static_library('bonjour', BONJOURSOURCES,
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_args : bonjour_link_args,
	    dependencies : [libxml, avahi, libpurple_dep, glib, ws2_32])
elif DYNAMIC_BONJOUR
	bonjour_prpl = shared_library('bonjour', BONJOURSOURCES,
	    link_args : bonjour_link_args,
	    dependencies : [libxml, avahi, libpurple_dep, glib, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
