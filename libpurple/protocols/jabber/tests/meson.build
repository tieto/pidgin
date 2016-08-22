# FIXME: 'jutil' can't find an inline function while linking.

foreach prog : ['caps', 'digest_md5', 'scram']
	e = executable(
	    'test_jabber_' + prog, 'test_jabber_@0@.c'.format(prog),
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : [jabber_prpl, libpurple],
	    dependencies : [libxml, gplugin, glib])

	test('jabber_' + prog, e)
endforeach
