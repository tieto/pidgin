DIALOGS = [
	'auth',
	'cool',
	'dialog',
	'error',
	'info',
	'mail',
	'question',
	'warning',
]

DIALOGS_16_ONLY = [
	'auth',
	'error',
	'info',
	'mail',
	'question',
]

foreach dialog : DIALOGS
	foreach size : ['16', '64', 'scalable']
		if size == 'scalable'
			filename = '@0@.svg'.format(dialog)
		else
			filename = '@0@.png'.format(dialog)
		endif
		if size != '16' or DIALOGS_16_ONLY.contains(dialog)
		install_data(join_paths(size, filename),
		    install_dir : join_paths(pidginpixmapdir, 'dialogs', size))
		endif
	endforeach
endforeach
