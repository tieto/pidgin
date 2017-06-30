PROGS = [
    'image',
    'smiley',
    'smiley_list',
    'trie',
    'util',
    'xmlnode'
]
foreach prog : PROGS
	e = executable('test_' + prog, 'test_@0@.c'.format(prog),
	               c_args : [
	                   '-DTEST_DATA_DIR="@0@/data"'.format(meson.current_source_dir())
	               ],
	               dependencies : [libpurple_dep, glib])
	test(prog, e)
endforeach
