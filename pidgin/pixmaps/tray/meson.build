TRAY = [
	'pidgin-tray-available',
	'pidgin-tray-away',
	'pidgin-tray-busy',
	'pidgin-tray-connect',
	'pidgin-tray-email',
	'pidgin-tray-invisible',
	'pidgin-tray-offline',
	'pidgin-tray-pending',
	'pidgin-tray-xa',
]

install_subdir('16',
    install_dir : pidginpixmapdir + '/tray')

install_data('hicolor/index.theme',
    install_dir : pidginpixmapdir + '/tray/hicolor')
if ENABLE_TRAYCOMPAT
	# Install in a location compatible with hicolor-icon-theme 0.9
	subdir = 'actions'
else
	# Install in the "correct" locations
	subdir = 'status'
endif
foreach tray : TRAY
	filename = '@0@.png'.format(tray)
	foreach size : [16, 22, 32, 48]
		size = '@0@x@0@'.format(size)
		inpath = join_paths('hicolor', size, 'status', filename)
		outpath = join_paths(pidginpixmapdir, 'tray', 'hicolor', size, subdir)
		install_data(inpath, install_dir : outpath)
	endforeach
endforeach
