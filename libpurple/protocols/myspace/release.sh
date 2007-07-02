#!/bin/bash
# Created:20070618
# By Jeff Connelly

# Package a new msimprpl for release. Must be run with bash.

VERSION=0.9
make
cd ..
# Include 'myspace' directory in archive, so it can easily be unextracted
# into ~/pidgin/libpurple/protocols at the correct location.
# (if this command fails, run it manually).
# This convenient command requires bash.
tar -cf msimprpl-$VERSION.tar myspace/{CHANGES,ChangeLog,LICENSE,Makefile*,*.c,*.h,README,release.sh,.deps/*}
cd myspace
gzip ../msimprpl-$VERSION.tar

mv ~/pidgin/config.h ~/pidgin/config.h-
make -f Makefile.mingw
mv ~/pidgin/config.h- ~/pidgin/config.h
cp ~/pidgin/win32-install-dir/plugins/libmyspace.dll .
# Zip is more common with Win32 users. Just include a few files in this archive.
zip ../msimprpl-$VERSION-win32.zip libmyspace.dll README LICENSE CHANGES
ls -l ../msimprpl-$VERSION*
