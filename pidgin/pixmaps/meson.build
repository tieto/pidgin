pidginpixmapdir = get_option('datadir') + '/pixmaps/pidgin'
pidginiconsdir = get_option('datadir') + '/icons/hicolor'

subdir('emotes/default/24')
subdir('emotes/small/16')

ICONS_16 = [
	'icons/hicolor/16x16/apps/pidgin.png'
]

ICONS_16_SCALABLE = [
	'icons/hicolor/16x16/apps/scalable/pidgin.svg'
]

ICONS_22 = [
	'icons/hicolor/22x22/apps/pidgin.png'
]

ICONS_24 = [
	'icons/hicolor/24x24/apps/pidgin.png'
]

ICONS_24_SCALABLE = [
	'icons/hicolor/24x24/apps/scalable/pidgin.svg'
]

ICONS_32 = [
	'icons/hicolor/32x32/apps/pidgin.png'
]

ICONS_32_SCALABLE = [
	'icons/hicolor/32x32/apps/scalable/pidgin.svg'
]

ICONS_48 = [
	'icons/hicolor/48x48/apps/pidgin.png'
]

ICONS_SCALABLE = [
	'icons/hicolor/scalable/apps/pidgin.svg'
]

TRAY_16 = [
	'tray/hicolor/16x16/status/pidgin-tray-away.png',
	'tray/hicolor/16x16/status/pidgin-tray-busy.png',
	'tray/hicolor/16x16/status/pidgin-tray-invisible.png',
	'tray/hicolor/16x16/status/pidgin-tray-connect.png',
	'tray/hicolor/16x16/status/pidgin-tray-xa.png',
	'tray/hicolor/16x16/status/pidgin-tray-email.png',
	'tray/hicolor/16x16/status/pidgin-tray-pending.png',
	'tray/hicolor/16x16/status/pidgin-tray-offline.png',
	'tray/hicolor/16x16/status/pidgin-tray-available.png'
]

TRAY_22 = [
	'tray/hicolor/22x22/status/pidgin-tray-away.png',
	'tray/hicolor/22x22/status/pidgin-tray-busy.png',
	'tray/hicolor/22x22/status/pidgin-tray-connect.png',
	'tray/hicolor/22x22/status/pidgin-tray-xa.png',
	'tray/hicolor/22x22/status/pidgin-tray-invisible.png',
	'tray/hicolor/22x22/status/pidgin-tray-email.png',
	'tray/hicolor/22x22/status/pidgin-tray-pending.png',
	'tray/hicolor/22x22/status/pidgin-tray-offline.png',
	'tray/hicolor/22x22/status/pidgin-tray-available.png'
]

TRAY_32 = [
	'tray/hicolor/32x32/status/pidgin-tray-away.png',
	'tray/hicolor/32x32/status/pidgin-tray-busy.png',
	'tray/hicolor/32x32/status/pidgin-tray-connect.png',
	'tray/hicolor/32x32/status/pidgin-tray-xa.png',
	'tray/hicolor/32x32/status/pidgin-tray-invisible.png',
	'tray/hicolor/32x32/status/pidgin-tray-email.png',
	'tray/hicolor/32x32/status/pidgin-tray-pending.png',
	'tray/hicolor/32x32/status/pidgin-tray-offline.png',
	'tray/hicolor/32x32/status/pidgin-tray-available.png'
]

TRAY_48 = [
	'tray/hicolor/48x48/status/pidgin-tray-away.png',
	'tray/hicolor/48x48/status/pidgin-tray-busy.png',
	'tray/hicolor/48x48/status/pidgin-tray-connect.png',
	'tray/hicolor/48x48/status/pidgin-tray-xa.png',
	'tray/hicolor/48x48/status/pidgin-tray-invisible.png',
	'tray/hicolor/48x48/status/pidgin-tray-email.png',
	'tray/hicolor/48x48/status/pidgin-tray-pending.png',
	'tray/hicolor/48x48/status/pidgin-tray-offline.png',
	'tray/hicolor/48x48/status/pidgin-tray-available.png'
]

if INSTALL_PIXMAPS
	install_data('edit.png', 'pause.png', 'info.png',
	    install_dir : pidginpixmapdir)

	install_data('logo.png', 'arrow-down.xpm', 'arrow-left.xpm', 'arrow-right.xpm', 'arrow-up.xpm',
	    install_dir : pidginpixmapdir)

	install_subdir('animations',
	    install_dir : pidginpixmapdir)
	install_subdir('dialogs',
	    install_dir : pidginpixmapdir)
	install_subdir('e2ee',
	    install_dir : pidginpixmapdir)
	install_subdir('emblems',
	    install_dir : pidginpixmapdir)
	install_subdir('protocols',
	    install_dir : pidginpixmapdir)
	install_subdir('status',
	    install_dir : pidginpixmapdir)
	install_subdir('toolbar',
	    install_dir : pidginpixmapdir)
	install_subdir('tray/16',
	    install_dir : pidginpixmapdir + '/tray')
	install_data('tray/hicolor/index.theme',
	    install_dir : pidginpixmapdir + '/tray/hicolor')

	if ENABLE_TRAYCOMPAT
		# Install in a location compatible with hicolor-icon-theme 0.9
		subdir = 'actions'
	else
		# Install in the "correct" locations
		subdir = 'status'
	endif
	install_data(TRAY_16,
	    install_dir : pidginpixmapdir + '/tray/hicolor/16x16/' + subdir)
	install_data(TRAY_22,
	    install_dir : pidginpixmapdir + '/tray/hicolor/22x22/' + subdir)
	install_data(TRAY_32,
	    install_dir : pidginpixmapdir + '/tray/hicolor/32x32/' + subdir)
	install_data(TRAY_48,
	    install_dir : pidginpixmapdir + '/tray/hicolor/48x48/' + subdir)

	# Don't use install_subdir because it deletes the target, and the
	# /usr/share/icons directory probably has something in it, for those
	# installing somewhere with existing stuff.
	foreach size : [16, 22, 24, 32, 48]
		install_data(get_variable('ICONS_@0@'.format(size)),
		    install_dir : '@0@/@1@x@1@/apps'.format(pidginiconsdir, size))
		if size != 22 and size != 48
			install_data(get_variable('ICONS_@0@_SCALABLE'.format(size)),
			    install_dir : '@0@/@1@x@1@/apps/scalable'.format(pidginiconsdir, size))
		endif
	endforeach
	install_data(ICONS_SCALABLE,
	    install_dir : pidginiconsdir + '/scalable/apps')
endif
