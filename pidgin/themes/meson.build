themetemplatedir = get_option('datadir') + '/pidgin/theme'
themecontentsdir = themetemplatedir + '/Contents'
themeresourcesdir = themecontentsdir + '/Resources'
themeresourcesincomingdir = themeresourcesdir + '/Incoming'
themevariantsdir = themeresourcesdir + '/Variants'

install_data('Template.html',
    install_dir : themetemplatedir)
install_data('Contents/Info.plist',
    install_dir : themecontentsdir)
install_data(['Contents/Resources/Content.html',
              'Contents/Resources/Status.html',
              'Contents/Resources/main.css'],
    install_dir : themeresourcesdir)
install_data('Contents/Resources/Incoming/Content.html',
    install_dir : themeresourcesincomingdir)
install_data(['Contents/Resources/Variants/Default.css',
              'Contents/Resources/Variants/No-Timestamps.css'],
    install_dir : themevariantsdir)
