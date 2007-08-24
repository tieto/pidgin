#!/bin/bash
# Created:20070618
# By Jeff Connelly

# Package a new msimprpl for release. Must be run with bash.

VERSION=0.16
make
# Include 'myspace' directory in archive, so it can easily be unextracted
# into ~/pidgin/libpurple/protocols at the correct location.
# (if this command fails, run it manually).
# This convenient command requires bash.
cd ../../..
tar -cf libpurple/protocols/msimprpl-$VERSION.tar libpurple/protocols/myspace/{CHANGES,ChangeLog,LICENSE,Makefile.*,*.c,*.h,README,release.sh,.deps/*} autogen.sh configure.ac
cd libpurple/protocols/myspace
gzip ../msimprpl-$VERSION.tar

mv ~/pidgin/config.h ~/pidgin/config.h-
make -f Makefile.mingw
mv ~/pidgin/config.h- ~/pidgin/config.h
cp ~/pidgin/win32-install-dir/plugins/libmyspace.dll .
# Zip is more common with Win32 users. Just include a few files in this archive,
# but (importantly) preserve the install directory structure!
mkdir -p win32-archive/plugins
cp libmyspace.dll win32-archive/plugins
mkdir -p win32-archive/pixmaps/pidgin/protocols/{48,22,16}
cp ~/pidgin/win32-install-dir/pixmaps/pidgin/protocols/48/myspace.png \
                win32-archive/pixmaps/pidgin/protocols/48/
cp ~/pidgin/win32-install-dir/pixmaps/pidgin/protocols/22/myspace.png \
                win32-archive/pixmaps/pidgin/protocols/22/
cp ~/pidgin/win32-install-dir/pixmaps/pidgin/protocols/16/myspace.png \
                win32-archive/pixmaps/pidgin/protocols/16/
mkdir -p win32-archive/pixmaps/pidgin/emotes/default
cp ~/pidgin/win32-install-dir/pixmaps/pidgin/emotes/default/theme \
        win32-archive/pixmaps/pidgin/emotes/default/theme
# Emoticons in MySpaceIM but not Pidgin 2.1.0
cp ~/pidgin/win32-install-dir/pixmaps/pidgin/emotes/default/{sinister,sidefrown,pirate,mohawk,messed,bulgy-eyes}.png \
	win32-archive/pixmaps/pidgin/emotes/default/

# Use DOS line endings and .txt file extension for convenience
u2d < README > win32-archive/msimprpl-README.txt
u2d < LICENSE > win32-archive/msimprpl-LICENSE.txt
u2d < CHANGES > win32-archive/msimprpl-CHANGES.txt
cd win32-archive
zip -r ../../msimprpl-$VERSION-win32.zip *
cd ..
rm -rf win32-archive
ls -l ../msimprpl-$VERSION*
