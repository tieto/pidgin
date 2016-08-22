foreach prog : ['util']
	e = executable(
	    'test_yahoo_' + prog, 'test_yahoo_@0@.c'.format(prog),
	    link_with : [yahoo_prpl],
	    dependencies : [libpurple_dep, glib])

	test('yahoo_' + prog, e)
endforeach
