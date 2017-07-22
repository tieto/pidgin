OSCARSOURCES = [
	'authorization.c',
	'aim.c',
	'aim.h',
	'bstream.c',
	'clientlogin.c',
	'kerberos.c',
	'encoding.c',
	'encoding.h',
	'family_admin.c',
	'family_alert.c',
	'family_auth.c',
	'family_bart.c',
	'family_bos.c',
	'family_buddy.c',
	'family_chat.c',
	'family_chatnav.c',
	'family_icq.c',
	'family_icbm.c',
	'family_locate.c',
	'family_oservice.c',
	'family_popup.c',
	'family_feedbag.c',
	'family_stats.c',
	'family_userlookup.c',
	'flap_connection.c',
	'icq.c',
	'icq.h',
	'misc.c',
	'msgcookie.c',
	'odc.c',
	'oft.c',
	'oscar.c',
	'oscar.h',
	'oscarcommon.h',
	'oscar_data.c',
	'peer.c',
	'peer.h',
	'peer_proxy.c',
	'rxhandlers.c',
	'snac.c',
	'snactypes.h',
	'tlv.c',
	'userinfo.c',
	'util.c',
	'visibility.c',
	'visibility.h'
]

if IS_WIN32
	oscar_link_args = ['-Wl,--export-all-symbols']
else
	oscar_link_args = []
endif

if STATIC_OSCAR
	oscar_prpl = static_library('oscar', OSCARSOURCES,
	    c_args : '-DPURPLE_STATIC_PRPL',
	    link_args : oscar_link_args,
	    dependencies : [libpurple_dep, glib, ws2_32])
elif DYNAMIC_OSCAR
	oscar_prpl = shared_library('oscar', OSCARSOURCES,
	    link_args : oscar_link_args,
	    dependencies : [libpurple_dep, glib, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif

subdir('tests')
