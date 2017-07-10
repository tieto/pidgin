ICONS = [
	'pidgin'
]

foreach icon : ICONS
	foreach size : [16, 22, 24, 32, 48]
		path = 'hicolor/@0@x@0@/apps'.format(size)
		install_data(join_paths(path, '@0@.png'.format(icon)),
		    install_dir : join_paths(get_option('datadir'), 'icons', path))
	endforeach
	path = 'hicolor/scalable/apps'
	install_data(join_paths(path, '@0@.svg'.format(icon)),
	    install_dir : join_paths(get_option('datadir'), 'icons', path))
endforeach
