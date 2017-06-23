if ENABLE_DOC
	subdir('reference')
endif

if ENABLE_GTK
	pidgin_man = configure_file(
	    input : 'pidgin.1.in',
	    output : 'pidgin.1',
	    configuration : man_conf,
	    install : true,
	    install_dir : get_option('mandir') + '/man1')
endif

if enable_consoleui
	finch_man = configure_file(
	    input : 'finch.1.in',
	    output : 'finch.1',
	    configuration : man_conf,
	    install : true,
	    install_dir : get_option('mandir') + '/man1')
endif
