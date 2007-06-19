#!/bin/sh
# Created:20070618
# By Jeff Connelly

VERSION=0.8
make
cd ..
# Include 'myspace' directory in archive, so it can easily be unextracted
# into ~/pidgin/libpurple/protocols at the correct location.
tar -cf msimprpl-$VERSION.tar myspace/
cd myspace
gzip ../msimprpl-$VERSION.tar

mv ~/pidgin/config.h ~/pidgin/config.h-
make -f Makefile.mingw
mv ~/pidgin/config.h- ~/pidgin/config.h
cp ~/pidgin/win32-install-dir/plugins/libmyspace.dll .
# Zip is more common with Win32 users
zip ../msimprpl-$VERSION-win32.zip libmyspace.dll README LICENSE
ls -l ../msimprpl-$VERSION*
