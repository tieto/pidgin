YAHOOSOURCES = [
	'util.c',
	'yahoochat.h',
	'yahoochat.c',
	'yahoo.c',
	'yahoo.h',
	'yahoo_aliases.c',
	'yahoo_aliases.h',
	'yahoo_doodle.h',
	'yahoo_doodle.c',
	'yahoo_filexfer.h',
	'yahoo_filexfer.c',
	'yahoo_friend.h',
	'yahoo_friend.c',
	'yahoo_packet.h',
	'yahoo_packet.c',
	'yahoo_picture.c',
	'yahoo_picture.h',
	'yahoo_profile.c',
	'ycht.c',
	'ycht.h',
	'ymsg.c',
	'ymsg.h'
]

if STATIC_YAHOO
	yahoo_prpl = static_library('yahoo', YAHOOSOURCES,
	    c_args : '-DPURPLE_STATIC_PRPL',
	    dependencies : [libpurple_dep, glib, gio])
elif DYNAMIC_YAHOO
	yahoo_prpl = shared_library('yahoo', YAHOOSOURCES,
	    dependencies : [libpurple_dep, glib, gio],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif

subdir('tests')
