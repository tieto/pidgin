foreach dir : ['16', '22', '48', 'scalable']
	if dir == 'scalable'
		install_subdir(dir,
		    install_dir : join_paths(pidginpixmapdir, 'protocols'))
	else
		install_subdir(dir,
		    exclude_directories : 'scalable',
		    install_dir : join_paths(pidginpixmapdir, 'protocols'))
	endif
endforeach
