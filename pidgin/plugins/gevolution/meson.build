gevolution_SOURCES = [
	'add_buddy_dialog.c',
	'assoc-buddy.c',
	'gevolution.c',
	'gevolution.h',
	'gevo-util.c',
	'new_person_dialog.c',
	'eds-utils.c'
]

if PLUGINS
	gevolution = library('gevolution', gevolution_SOURCES,
	    dependencies : EVOLUTION_ADDRESSBOOK + [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
