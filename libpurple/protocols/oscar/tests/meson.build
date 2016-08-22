foreach prog : ['util']
	e = executable(
	    'test_oscar_' + prog, 'test_oscar_@0@.c'.format(prog),
	    link_with : [oscar_prpl],
	    dependencies : [libpurple_dep, glib])

	test('oscar_' + prog, e)
endforeach
