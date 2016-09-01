if USE_GCONFTOOL
#	subdir('data/gconf')
endif

purple_coresources = [
	'account.c',
	'accounts.c',
	'accountopt.c',
	'blistnode.c',
	'buddy.c',
	'buddylist.c',
	'buddyicon.c',
	'chat.c',
	'ciphers/aescipher.c',
	'ciphers/descipher.c',
	'ciphers/des3cipher.c',
	'ciphers/hmaccipher.c',
	'ciphers/md4hash.c',
	'ciphers/md5hash.c',
	'ciphers/pbkdf2cipher.c',
	'ciphers/rc4cipher.c',
	'ciphers/sha1hash.c',
	'ciphers/sha256hash.c',
	'cipher.c',
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
	'ntlm.c',
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
	'purple-socket.c',
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
	'cipher.h',
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
	'ntlm.h',
	'plugins.h',
	'pluginpref.h',
	'pounce.h',
	'prefs.h',
	'presence.h',
	'proxy.h',
	'protocol.h',
	'protocols.h',
	'purple-gio.h',
	'purple-socket.h',
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

if USE_VV
	purple_coreheaders += 'media-gst.h'
endif

if IS_WIN32
	purple_coresources += [
		'win32/giowin32.c',
		'win32/libc_interface.c',
		'win32/win32dep.c'
	]

	purple_coreheaders += [
		'win32/libc_interface.h',
		'win32/libc_internal.h',
		'win32/win32dep.h',
		'win32/wpurpleerror.h'
	]

	# TODO: Do something with this object.
	LIBPURPLE_WIN32RES = WINDRES.process('win32/libpurplerc.rc')
endif

purple_mediaheaders = [
	'media/backend-iface.h',
	'media/candidate.h',
	'media/codec.h',
	'media/enum-types.h'
]

purple_cipherheaders = [
	'ciphers/aescipher.h',
	'ciphers/descipher.h',
	'ciphers/des3cipher.h',
	'ciphers/hmaccipher.h',
	'ciphers/md4hash.h',
	'ciphers/md5hash.h',
	'ciphers/pbkdf2cipher.h',
	'ciphers/rc4cipher.h',
	'ciphers/sha1hash.h',
	'ciphers/sha256hash.h'
]

purple_enumheaders = [
	'account.h',
	'buddyicon.h',
	'cipher.h',
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


marshallers = gnome.genmarshal('marshallers',
    sources : 'marshallers.list',
    prefix : 'purple_smarshal')
marshallers_c = marshallers[0]
marshallers_h = marshallers[1]

enums_h = gnome.mkenums('enums.h',
    sources : purple_enumheaders,
    template : 'enums.h.in',
    install : true,
    install_dir : get_option('includedir') + '/libpurple')
enums_c = gnome.mkenums('enums.c',
    sources : purple_enumheaders,
    template : 'enums.c.in')

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
	marshallers_c
]

purple_builtheaders = [
	purple_h,
	version_h,
	enums_h,  # FIXME: Custom targets don't work as dependencies.
	marshallers_h
]

if ENABLE_DBUS

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

purple_build_coreheaders = purple_coreheaders + purple_cipherheaders + purple_mediaheaders
purple_build_coreheaders += purple_builtheaders


# We should probably make this better
dbus_signals = purple_coresources + ['protocols/irc/irc.c', 'protocols/jabber/jabber.c']

dbus_analyze_functions = files('dbus-analyze-functions.py')[0]
dbus_analyze_signals = files('dbus-analyze-signals.py')[0]
dbus_analyze_types = files('dbus-analyze-types.py')[0]

dbus_types_c = custom_target('dbus_types_c',
                             input : purple_build_coreheaders,
                             output : 'dbus-types.c',
                             command : [python, dbus_analyze_types, '-o', '@OUTPUT@',
                                        '--pattern=PURPLE_DBUS_DEFINE_TYPE(%s)', '@INPUT@'])

dbus_types_h = custom_target('dbus_types_h',
                             input : purple_build_coreheaders,
                             output : 'dbus-types.h',
                             command : [python, dbus_analyze_types, '-o', '@OUTPUT@',
                                        '--pattern=PURPLE_DBUS_DECLARE_TYPE(%s)', '@INPUT@'])

dbus_bindings_c = custom_target('dbus_bindings_c',
                                input : dbus_exported,
                                output : 'dbus-bindings.c',
                                command : [python, dbus_analyze_functions, '-o', '@OUTPUT@',
                                           '@INPUT@'])

dbus_signals_c = custom_target('dbus_signals_c',
                               input : dbus_signals,
                               output : 'dbus-signals.c',
                               command : [python, dbus_analyze_signals, '-o', '@OUTPUT@',
                                          '@INPUT@'])

# Automatic?
#dbus-server.$(OBJEXT): dbus-bindings.c dbus-signals.c dbus-types.c dbus-types.h
#dbus-server.lo: dbus-bindings.c dbus-signals.c dbus-types.c dbus-types.h
#$(libpurple_la_OBJECTS): dbus-types.h


# libpurple-client

purple_client_bindings_c = custom_target('purple_client_bindings_c',
                                         input : dbus_exported,
                                         output : 'purple-client-bindings.c',
                                         command : [python, meson.current_source_dir() + '/dbus-analyze-functions.py',
                                                    '--client', '-o', '@OUTPUT@', '@INPUT@'])

#purple-client-bindings.h: dbus-analyze-types.py dbus-analyze-functions.py $(purple_coreheaders) $(addprefix ciphers/, $(purple_cipherheaders)) $(addprefix media/, $(purple_mediaheaders)) $(purple_builtheaders) $(dbus_exported)
#	$(PYTHON) $(srcdir)/dbus-analyze-types.py --keyword=enum --verbatim -o $@ $(purple_build_coreheaders)
#	$(PYTHON) $(srcdir)/dbus-analyze-functions.py --client --headers --append -o $@ $(dbus_exported)
#FIXME: Don't know how to run multiple commands like this.
#purple_client_bindings_h = custom_target('purple_client_bindings_h',
#                                         input : [purple_build_coreheaders, dbus_exported],
#                                         output : 'purple-client-bindings.h',
#                                         command : [
#                                             python, meson.current_source_dir() + '/dbus-analyze-types.py',
#                                              '--keyword=enum', '--verbatim', '-o', '@OUTPUT@', '@INPUT0@', '&&',
#                                             python, meson.current_source_dir() + '/dbus-analyze-functions.py',
#                                              '--client', '--headers', '--append', '-o', '@OUTPUT@', '@INPUT1@']
#                                         )

# Automatic?
#$(libpurple_client_la_OBJECTS): purple-client-bindings.h purple-client-bindings.c

#libpurple_client = library('purple-client', 'purple-client.c',
#                           soversion : purple_lt_current,
#                           version : PURPLE_LT_VERSION_INFO,
#                           dependencies : [dbus, dbus_glib])

# scripts

#bin_SCRIPTS = purple-remote purple-send purple-send-async purple-url-handler

else

dbus_sources = []

endif

libpurple_inc = include_directories('.')
libpurple = library('purple',
                    purple_coresources + purple_builtsources + dbus_sources + purple_builtheaders,
                    include_directories : [toplevel_inc, libpurple_inc],
                    install : true,
                    soversion : purple_lt_current,
                    version : PURPLE_LT_VERSION_INFO,
                    dependencies : # static_link_libs
                        [dbus, dbus_glib, dnsapi, glib, gio, gplugin, libxml, #networkmanager,
                         #intllibs,
                         farstream, gstreamer, gstreamer_video,
                         gstreamer_app, idn, json, gnutls,
                         nss, zlib, math])
libpurple_dep = declare_dependency(
    include_directories : [toplevel_inc, libpurple_inc],
    link_with : libpurple,
    dependencies : [gstreamer, gplugin, glib, gio])

if ENABLE_DBUS

# purple-client-example

#purple_client_example = executable('purple-client-example',
#                                   'purple-client-example.c',
#                                   link_with : [libpurple_client, libpurple],
#                                   dependencies : [dbus, dbus_glib, glib])

endif

#noinst_HEADERS= \
#	internal.h \
#	media/backend-fs2.h \
#	valgrind.h

install_headers(purple_coreheaders + dbus_headers,
                subdir : 'libpurple')

install_headers(purple_cipherheaders,
                subdir : 'libpurple/ciphers')

install_headers(purple_mediaheaders,
                subdir : 'libpurple/media')

configure_file(input : 'data/purple-3.pc.in',
               output : 'purple-3.pc',
               configuration : conf,
               install : true,
               install_dir : get_option('libdir') + '/pkgconfig')

if enable_introspection
	introspection_sources = (purple_coreheaders + purple_builtheaders +
	                         dbus_headers + purple_cipherheaders +
	                         purple_mediaheaders)

	Purple_gir_includes = ['GObject-2.0']
	if PLUGINS
		Purple_gir_includes += ['GPlugin-0.0']
	endif
	if ENABLE_DBUS
		Purple_gir_includes += ['DBus-1.0', 'DBusGLib-1.0']
	endif

	libpurple_gir = gnome.generate_gir(libpurple,
	    sources : introspection_sources,
	    includes : Purple_gir_includes,
	    namespace : 'Purple',
	    symbol_prefix : 'purple_',
	    identifier_prefix : 'Purple',
	    nsversion : '@0@.@1@'.format(purple_major_version,
	                                 purple_minor_version),
	    install : true)
endif

subdir('tests')
subdir('example')
subdir('plugins')
subdir('protocols')

