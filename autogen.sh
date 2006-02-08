#!/bin/sh

SETUP_GETTEXT=./setup-gettext

($SETUP_GETTEXT --gettext-tool) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have gettext installed to compile Gaim";
	echo;
	exit;
}

(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have libtool installed to compile Gaim";
	echo;
	exit;
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have automake installed to compile Gaim";
	echo;
	exit;
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have autoconf installed to compile Gaim";
	echo;
	exit;
}

echo "Generating configuration files for Gaim, please wait...."
echo;

# Backup the po/ChangeLog. This should prevent the annoying
# gettext ChangeLog modifications.

cp -p po/ChangeLog po/ChangeLog.save

echo "Running gettextize, please ignore non-fatal messages...."
$SETUP_GETTEXT

# Restore the po/ChangeLog file.
mv po/ChangeLog.save po/ChangeLog

echo "Running libtoolize, please ignore non-fatal messages...."
echo n | libtoolize --copy --force || exit;

# Add other directories to this list if people continue to experience
# brokennesses ...  Obviously the real answer is for them to fix it
# themselves, but for Luke's sake we have this.
for dir in "/usr/local/share/aclocal" \
           "/opt/gnome-1.4/share/aclocal"
do
	if test -d $dir ; then
		ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $dir"
	fi
done

libtoolize -c -f --automake
intltoolize --force --copy
aclocal $ACLOCAL_FLAGS -I ./m4 || exit;
autoheader || exit;
automake --add-missing --copy;
autoconf || exit;
automake || exit;
./configure $@

