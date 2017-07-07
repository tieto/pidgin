DOC_MODULE = 'facebook'

# Header files or dirs to ignore when scanning. Use base file/dir names
ignore_hfiles = [
]

# Extra options to supply to gtkdoc-scan.
scan_args = [
    '--deprecated-guards=PURPLE_DISABLE_DEPRECATED',
    '--rebuild-types',
    '--rebuild-sections',
    '--ignore-headers=' + ' '.join(ignore_hfiles),
]

# Extra SGML files that are included by $(DOC_MAIN_XML_FILE).
content_files = [
    libpurple_version_xml,
]

gnome.gtkdoc(DOC_MODULE,
    main_xml : DOC_MODULE + '-docs.xml',
    src_dir : facebook_inc,
    dependencies : facebook_dep,
    install : true,
    scan_args : scan_args,
    gobject_typesfile : DOC_MODULE + '.types',
    content_files : content_files)
