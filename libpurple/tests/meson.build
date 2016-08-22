foreach prog : ['des', 'des3', 'hmac', 'md4', 'md5', 'sha1', 'sha256', 'trie', 'util', 'xmlnode']
	e = executable('test_' + prog, 'test_@0@.c'.format(prog),
	               dependencies : [libpurple_dep, glib])
	test(prog, e)
endforeach
