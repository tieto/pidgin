DOC_MODULE = 'libpurple'

# Header files or dirs to ignore when scanning. Use base file/dir names
ignore_hfiles = [
    'example',
    'plugins',
    'protocols',
    'tests',
    'win32',
    'backend-fs2.h',
    'dbus-define-api.h',
    'dbus-types.h',
    'glibcompat.h',
    'internal.h',
    'purple-client.h',
    'purple-client-bindings.h',
    'valgrind.h'
]

# Extra options to supply to gtkdoc-scan.
scan_args = [
    '--deprecated-guards=PURPLE_DISABLE_DEPRECATED',
    '--rebuild-types',
    '--rebuild-sections',
    '--ignore-headers=' + ' '.join(ignore_hfiles),
]

# Extra options to supply to gtkdoc-mkdb.
mkdb_args = [
    '--ignore-files=' + ' '.join(ignore_hfiles),
]

libpurple_version_xml = configure_file(
    input : 'version.xml.in',
    output : 'version.xml',
    configuration : version_conf)

content_files = [
    'plugin_i18n.xml',
    'plugin_ids.xml',
    'signals_account.xml',
    'signals_blist.xml',
    'signals_certificate.xml',
    'signals_cmd.xml',
    'signals_connection.xml',
    'signals_conversation.xml',
    'signals_core.xml',
    'signals_dbus_server.xml',
    'signals_jabber.xml',
    'signals_log.xml',
    'signals_notify.xml',
    'signals_plugin.xml',
    'signals_protocol.xml',
    'signals_savedstatus.xml',
    'signals_sound.xml',
    'signals_xfer.xml',
    'tut_c_plugins.xml',
    'tut_signals.xml',
    'ui_ops.xml',
]

gnome.gtkdoc(DOC_MODULE,
    main_xml : DOC_MODULE + '-docs.xml',
    src_dir : libpurple_inc,
    dependencies : libpurple_dep,
    install : true,
    scan_args : scan_args,
    mkdb_args : mkdb_args,
    gobject_typesfile : DOC_MODULE + '.types',
    content_files : content_files)
