#!/bin/sh

abort() {
	# Don't break the tree if something goes wrong.
	if [ -e m4~ ]; then
		rm -rf m4
		mv m4~ m4
	fi

	exit 1
}

(gettextize --version) < /dev/null > /dev/null 2>&1 || {
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

# Thanks decklin
if test -f configure.ac ; then
	if autoconf --version | grep '2\.[01]' > /dev/null 2>&1 ; then
		mv configure.ac configure.2.1x;
		echo "configure.ac has been moved to configure.2.1x to retain compatibility with autoconf 2.1x"
		echo "Future versions of Gaim will not support autoconf versions older than 2.50"

	fi
fi

echo "Generating configuration files for Gaim, please wait...."
echo;

echo "Running gettextize, please ignore non-fatal messages...."

# Get the major version of gettext.
GETTEXT_VER=`gettextize --version | sed -n 's/^.*[0-9]\+\.\([0-9]\+\)\..*$/\1/p'`

# Decide how we want to run gettext.
if [ $GETTEXT_VER -eq 11 ]; then
	mv -f m4 m4~

	# Gettext is pure evil. It DEMANDS that we press Return no matter
	# what. This gets rid of their happy "feature" of doom.
	sed 's:read .*< /dev/tty::' `which gettextize` > gaim-gettextize
	chmod +x gaim-gettextize
	echo n | ./gaim-gettextize --copy --force --intl --no-changelog || abort
	rm gaim-gettextize

	# Now restore the things that brain-dead gettext modified.
	[ -e configure.in~ ] && mv -f configure.in~ configure.in
	[ -e configure.ac~ ] && mv -f configure.ac~ configure.ac
	[ -e Makefile.am~ ]  && mv -f Makefile.am~  Makefile.am
	rm -rf m4
	mv -f m4~ m4

	mv -f po/Makevars.template po/Makevars
else
	echo n | gettextize --copy --force || exit;
fi
echo "Running libtoolize, please ignore non-fatal messages...."
echo n | libtoolize --copy --force || exit;

aclocal -I m4 $ACLOCAL_FLAGS || exit;
autoheader || exit;
automake --add-missing --copy;
autoconf || exit;
automake || exit;
./configure $@

