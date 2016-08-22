foreach prog : ['des', 'des3', 'hmac', 'md4', 'md5', 'sha1', 'sha256', 'trie', 'util', 'xmlnode']
	e = executable('test_' + prog, 'test_@0@.c'.format(prog),
	               include_directories : [toplevel_inc, libpurple_inc],
	               link_with : libpurple,
	               dependencies : [gplugin, glib])
	test(prog, e)
endforeach

#AM_CPPFLAGS = \
#	-I$(top_srcdir)/libpurple \
#	-I$(top_builddir)/libpurple \
#	$(DEBUG_CFLAGS) \
#	$(GLIB_CFLAGS) \
#	$(GPLUGIN_CFLAGS) \
#	$(PLUGIN_CFLAGS) \
#	$(DBUS_CFLAGS) \
#	$(NSS_CFLAGS)
