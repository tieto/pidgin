nullclient = executable('nullclient', 'nullclient.c', 'defines.h',
                        include_directories : include_directories('..'),
                        link_with : libpurple,
                        dependencies : [dbus, dbus_glib, # intllibs,
                                        glib, gplugin, libxml],
                        c_args : ['-DSTANDALONE'])
