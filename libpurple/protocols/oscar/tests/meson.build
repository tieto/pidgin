foreach prog : ['util']
	e = executable(
	    'test_oscar_' + prog, 'test_oscar_@0@.c'.format(prog),
	    include_directories : [toplevel_inc, libpurple_inc],
	    link_with : [oscar_prpl, libpurple],
	    dependencies : [gplugin, glib])

	test('oscar_' + prog, e)
endforeach
