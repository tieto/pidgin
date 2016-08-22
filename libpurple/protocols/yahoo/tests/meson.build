foreach prog : ['util']
	e = executable(
	    'test_yahoo_' + prog, 'test_yahoo_@0@.c'.format(prog),
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : [yahoo_prpl, libpurple],
	    dependencies : [gplugin, glib])

	test('yahoo_' + prog, e)
endforeach
