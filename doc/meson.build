if ENABLE_GTK_DOC
#	subdir('reference')
endif

if ENABLE_GTK
	pidgin_man = configure_file(
	    input : 'pidgin.1.in',
	    output : 'pidgin.1',
	    configuration : conf,
	    install : true,
	    install_dir : get_option('mandir') + '/man1')
endif

if ENABLE_GNT
	finch_man = configure_file(
	    input : 'finch.1.in',
	    output : 'finch.1',
	    configuration : conf,
	    install : true,
	    install_dir : get_option('mandir') + '/man1')
endif
