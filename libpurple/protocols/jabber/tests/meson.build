foreach prog : ['caps', 'digest_md5', 'scram', 'jutil']
	e = executable(
	    'test_jabber_' + prog, 'test_jabber_@0@.c'.format(prog),
	    link_with : [jabber_prpl],
	    dependencies : [libxml, libpurple_dep, glib])

	test('jabber_' + prog, e)
endforeach
