DOC_MODULE = 'finch'

# Header files or dirs to ignore when scanning. Use base file/dir names
ignore_hfiles = [
    'plugins',
    'test',
    'libgnt/wms',
    'gntinternal.h',
    'gnt-skel.h'
]

# Extra options to supply to gtkdoc-scan.
scan_args = [
    '--deprecated-guards=PURPLE_DISABLE_DEPRECATED|GNT_DISABLE_DEPRECATED|FINCH_DISABLE_DEPRECATED',
    '--rebuild-types',
    '--rebuild-sections',
    '--ignore-headers=' + ' '.join(ignore_hfiles),
]

# Extra options to supply to gtkdoc-mkdb.
mkdb_args = [
    '--ignore-files=' + ' '.join(ignore_hfiles),
]

configure_file(
    input : 'version.xml.in',
    output : 'version.xml',
    configuration : version_conf)

gnome.gtkdoc(DOC_MODULE,
    main_xml : DOC_MODULE + '-docs.xml',
    src_dir : libfinch_inc,
    dependencies : libfinch_dep,
    install : true,
    scan_args : scan_args,
    mkdb_args : mkdb_args,
    gobject_typesfile : DOC_MODULE + '.types')
