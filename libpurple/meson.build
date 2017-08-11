purple_coresources = [
	'account.c',
	'accounts.c',
	'accountopt.c',
	'blistnode.c',
	'buddy.c',
	'buddylist.c',
	'buddyicon.c',
	'chat.c',
	'circularbuffer.c',
	'cmds.c',
	'connection.c',
	'contact.c',
	'conversation.c',
	'conversationtypes.c',
	'conversations.c',
	'core.c',
	'countingnode.c',
	'debug.c',
	'e2ee.c',
	'eventloop.c',
	'group.c',
	'http.c',
	'idle.c',
	'image.c',
	'image-store.c',
	'keyring.c',
	'log.c',
	'media/backend-fs2.c',
	'media/backend-iface.c',
	'media/candidate.c',
	'media/codec.c',
	'media/enum-types.c',
	'media.c',
	'mediamanager.c',
	'memorypool.c',
	'message.c',
	'mime.c',
	'nat-pmp.c',
	'network.c',
	'notify.c',
	'plugins.c',
	'pluginpref.c',
	'pounce.c',
	'prefs.c',
	'presence.c',
	'proxy.c',
	'protocol.c',
	'protocols.c',
	'purple-gio.c',
	'queuedoutputstream.c',
	'request.c',
	'request-datasheet.c',
	'roomlist.c',
	'savedstatuses.c',
	'server.c',
	'signals.c',
	'smiley-custom.c',
	'smiley-list.c',
	'smiley-parser.c',
	'smiley-theme.c',
	'smiley.c',
	'status.c',
	'stringref.c',
	'stun.c',
	'sound.c',
	'sound-theme.c',
	'sound-theme-loader.c',
	'sslconn.c',
	'theme.c',
	'theme-loader.c',
	'theme-manager.c',
	'tls-certificate.c',
	'tls-certificate-info.c',
	'trie.c',
	'upnp.c',
	'util.c',
	'version.c',
	'whiteboard.c',
	'xfer.c',
	'xmlnode.c'
]

purple_coreheaders = [
	'account.h',
	'accounts.h',
	'accountopt.h',
	'blistnode.h',
	'buddy.h',
	'buddylist.h',
	'buddyicon.h',
	'chat.h',
	'circularbuffer.h',
	'cmds.h',
	'connection.h',
	'contact.h',
	'conversation.h',
	'conversationtypes.h',
	'conversations.h',
	'core.h',
	'countingnode.h',
	'dbus-maybe.h',
	'debug.h',
	'e2ee.h',
	'eventloop.h',
	'group.h',
	'http.h',
	'idle.h',
	'image.h',
	'image-store.h',
	'keyring.h',
	'log.h',
	'media.h',
	'mediamanager.h',
	'memorypool.h',
	'message.h',
	'mime.h',
	'nat-pmp.h',
	'network.h',
	'notify.h',
	'plugins.h',
	'pluginpref.h',
	'pounce.h',
	'prefs.h',
	'presence.h',
	'proxy.h',
	'protocol.h',
	'protocols.h',
	'purple-gio.h',
	'queuedoutputstream.h',
	'request.h',
	'request-datasheet.h',
	'roomlist.h',
	'savedstatuses.h',
	'server.h',
	'signals.h',
	'smiley-custom.h',
	'smiley-list.h',
	'smiley-parser.h',
	'smiley-theme.h',
	'smiley.h',
	'status.h',
	'stringref.h',
	'stun.h',
	'sound.h',
	'sound-theme.h',
	'sound-theme-loader.h',
	'sslconn.h',
	'tests.h',
	'theme.h',
	'theme-loader.h',
	'theme-manager.h',
	'tls-certificate.h',
	'tls-certificate-info.h',
	'trie.h',
	'upnp.h',
	'util.h',
	'whiteboard.h',
	'xfer.h',
	'xmlnode.h',
]

if enable_vv
	purple_coreheaders += 'media-gst.h'
endif

if IS_WIN32
	purple_coresources += [
		'win32/libc_interface.c',
		'win32/win32dep.c'
	]

	purple_coreheaders += [
		'win32/libc_interface.h',
		'win32/libc_internal.h',
		'win32/win32dep.h',
		'win32/wpurpleerror.h'
	]

	libpurplerc = configure_file(
	    input : 'win32/libpurplerc.rc.in',
	    output : 'libpurplerc.rc',
	    configuration : version_conf)
	purple_coresources += windows.compile_resources(libpurplerc)
endif

purple_mediaheaders = [
	'media/backend-iface.h',
	'media/candidate.h',
	'media/codec.h',
	'media/enum-types.h'
]

purple_enumheaders = [
	'account.h',
	'buddyicon.h',
	'connection.h',
	'conversation.h',
	'conversationtypes.h',
	'debug.h',
	'eventloop.h',
	'notify.h',
	'plugins.h',
	'protocol.h',
	'protocols.h',
	'roomlist.h',
	'status.h',
	'sound.h',
	'xfer.h',
	'xmlnode.h'
]


enums = gnome.mkenums('enums',
    sources : purple_enumheaders,
    h_template : 'enums.h.in',
    c_template : 'enums.c.in',
    install_header : true,
    install_dir : get_option('includedir') + '/libpurple')
enums_c = enums[0]
enums_h = enums[1]

purple_h = configure_file(input : 'purple.h.in',
                          output : 'purple.h',
                          configuration : conf,
                          install : true,
                          install_dir : get_option('includedir') + '/libpurple')
version_h = configure_file(input : 'version.h.in',
                           output : 'version.h',
                           configuration : version_conf,
                           install : true,
                           install_dir : get_option('includedir') + '/libpurple')

purple_builtsources = [
	enums_c,
]

purple_builtheaders = [
	purple_h,
	version_h,
	enums_h,
]

if enable_dbus

# purple dbus server

dbus_sources = [
	'dbus-server.c',
	'dbus-useful.c'
]
dbus_headers = [
	'dbus-server.h',
	'dbus-bindings.h',
	'dbus-purple.h',
	'dbus-useful.h',
	'dbus-define-api.h',
]
dbus_exported = [
	'dbus-useful.h',
	'dbus-define-api.h',
	'account.h',
	'accounts.h',
	'blistnode.h',
	'buddy.h',
	'buddylist.h',
	'buddyicon.h',
	'connection.h',
	'conversation.h',
	'conversationtypes.h',
	'conversations.h',
	'core.h',
	'xfer.h',
	'log.h',
	'notify.h',
	'prefs.h',
	'presence.h',
	'roomlist.h',
	'savedstatuses.h',
	'smiley.h',
	'smiley-list.h',
	'status.h',
	'server.h',
	'util.h',
	'xmlnode.h',
	'protocol.h',
	'protocols.h'
]

purple_build_coreheaders = purple_coreheaders + purple_mediaheaders
purple_build_coreheaders += purple_builtheaders


# We should probably make this better
dbus_signals = purple_coresources + ['protocols/irc/irc.c', 'protocols/jabber/jabber.c']

dbus_analyze_functions = files('dbus-analyze-functions.py')[0]
dbus_analyze_signals = files('dbus-analyze-signals.py')[0]
dbus_analyze_types = files('dbus-analyze-types.py')[0]

dbus_types_c = custom_target('dbus_types_c',
                             input : purple_build_coreheaders,
                             output : 'dbus-types.ch',
                             command : [python, dbus_analyze_types, '-o', '@OUTPUT@',
                                        '--pattern=PURPLE_DBUS_DEFINE_TYPE(%s)', '@INPUT@'])

dbus_types_h = custom_target('dbus_types_h',
                             input : purple_build_coreheaders,
                             output : 'dbus-types.h',
                             command : [python, dbus_analyze_types, '-o', '@OUTPUT@',
                                        '--pattern=PURPLE_DBUS_DECLARE_TYPE(%s)', '@INPUT@'],
                             install : true,
                             install_dir : join_paths(get_option('includedir'), 'libpurple'))

dbus_bindings_c = custom_target('dbus_bindings_c',
                                input : dbus_exported,
                                output : 'dbus-bindings.ch',
                                command : [python, dbus_analyze_functions, '-o', '@OUTPUT@',
                                           '@INPUT@'])

dbus_signals_c = custom_target('dbus_signals_c',
                               input : dbus_signals,
                               output : 'dbus-signals.ch',
                               command : [python, dbus_analyze_signals, '-o', '@OUTPUT@',
                                          '@INPUT@'])

dbus_builtheaders = [dbus_types_h]
dbus_builtsources = [dbus_types_c, dbus_bindings_c, dbus_signals_c]

# libpurple-client

purple_client_bindings_h1 = custom_target('purple_client_bindings_h1',
    input : purple_build_coreheaders,
    output : 'purple-client-bindings1.h',
    command : [
        python, dbus_analyze_types,
        '--keyword=enum', '--verbatim', '-o', '@OUTPUT@', '@INPUT@'
    ]
)

purple_client_bindings_h2 = custom_target('purple_client_bindings_h2',
    input : dbus_exported,
    output : 'purple-client-bindings2.h',
    command : [
        python, dbus_analyze_functions,
        '--client', '--headers', '-o', '@OUTPUT@', '@INPUT@'
    ]
)

purple_client_bindings_h = custom_target('purple_client_bindings_h',
    input : [purple_client_bindings_h1, purple_client_bindings_h2],
    output : 'purple-client-bindings.h',
    capture : true,
    command : ['cat', '@INPUT@'])

purple_client_bindings_c = custom_target('purple_client_bindings_c',
    input : dbus_exported,
    output : 'purple-client-bindings.ch',
    command : [
        python, dbus_analyze_functions,
        '--client', '-o', '@OUTPUT@', '@INPUT@'
    ]
)

libpurple_client = library('purple-client',
    'purple-client.c', purple_client_bindings_c, purple_client_bindings_h,
    version : PURPLE_LIB_VERSION,
    dependencies : [dbus, dbus_glib],
    install : true)

# scripts

install_data(['purple-remote', 'purple-send', 'purple-send-async', 'purple-url-handler'],
    install_dir : get_option('bindir'))

else

dbus_sources = []
dbus_headers = []
dbus_builtsources = []
dbus_builtheaders = []

endif

libpurple_inc = include_directories('.')
libpurple = library('purple',
                    purple_coresources + purple_builtsources +
                    dbus_sources + dbus_builtsources +
                    purple_builtheaders + dbus_builtheaders,
                    include_directories : [toplevel_inc, libpurple_inc],
                    install : true,
                    version : PURPLE_LIB_VERSION,
                    dependencies : # static_link_libs
                        [dbus, dbus_glib, dnsapi, ws2_32, glib, gio, gplugin, libxml,
                         farstream, gstreamer, gstreamer_video,
                         gstreamer_app, idn, json, math])
libpurple_dep = declare_dependency(
    sources : purple_builtheaders,  # Ensure built before any dependencies.
    include_directories : [toplevel_inc, libpurple_inc],
    link_with : libpurple,
    dependencies : [gstreamer, gplugin, glib, gio])

if enable_dbus

# purple-client-example

purple_client_example = executable('purple-client-example',
    'purple-client-example.c', purple_client_bindings_h,
    link_with : [libpurple_client, libpurple],
    dependencies : [dbus, dbus_glib, glib],
    install : true)

endif

install_headers(purple_coreheaders + dbus_headers,
                subdir : 'libpurple')

install_headers(purple_mediaheaders,
                subdir : 'libpurple/media')

configure_file(input : 'data/purple-3.pc.in',
               output : 'purple-3.pc',
               configuration : pkg_conf,
               install : true,
               install_dir : get_option('libdir') + '/pkgconfig')

if INSTALL_I18N
	DESKTOP_FILE = 'purple-url-handler.desktop'
	desktop_file_in = configure_file(
	    input : 'data/' + DESKTOP_FILE + '.in.in',
	    output : DESKTOP_FILE + '.in',
	    configuration : conf)
	desktop_file = custom_target(DESKTOP_FILE,
	    input : desktop_file_in,
	    output : DESKTOP_FILE,
	    command : [intltool_merge, '--desktop-style', '-u',
	               '-c', '@BUILD_DIR@/po/.intltool-merge-cache',
	               meson.source_root() + '/po', '@INPUT@', '@OUTPUT@'],
	    install : true,
	    install_dir : get_option('datadir') + '/applications')
endif  # INSTALL_I18N

if enable_introspection
	introspection_sources = (purple_coreheaders + purple_builtheaders +
	                         dbus_headers + purple_mediaheaders)

	Purple_gir_includes = ['GObject-2.0', 'Gio-2.0']
	if PLUGINS
		Purple_gir_includes += ['GPlugin-0.0']
	endif
	if enable_dbus
		Purple_gir_includes += ['DBus-1.0', 'DBusGLib-1.0']
	endif
	if enable_gst
		Purple_gir_includes += ['Gst-1.0']
	endif

	libpurple_gir = gnome.generate_gir(libpurple,
	    sources : introspection_sources,
	    includes : Purple_gir_includes,
	    namespace : 'Purple',
	    symbol_prefix : 'purple',
	    identifier_prefix : 'Purple',
	    export_packages : 'purple-@0@'.format(purple_major_version),
	    nsversion : '@0@.@1@'.format(purple_major_version,
	                                 purple_minor_version),
	    install : true)
endif

subdir('tests')
subdir('example')
subdir('plugins')
subdir('protocols')
