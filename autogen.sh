#!/bin/sh

CONFIGURE_ARGS=""
if [ -f configure.args ] ; then
	. configure.args
fi

(glib-gettextize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have glib-gettextize installed to compile Pidgin.";
	echo;
	exit;
}

(intltoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have intltool installed to compile Pidgin.";
	echo;
	exit;
}

(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have libtool installed to compile Pidgin.";
	echo;
	exit;
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have automake installed to compile Pidgin.";
	echo;
	exit;
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo;
	echo "You must have autoconf installed to compile Pidgin.";
	echo;
	exit;
}

echo "Generating configuration files for Pidgin, please wait...."
echo;

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

echo "stage 1"
libtoolize -c -f --automake
echo "stage 2"
glib-gettextize --force --copy
echo "stage 3"
intltoolize --force --copy
echo "stage 4"
aclocal $ACLOCAL_FLAGS || exit;
echo "stage 5"
autoheader || exit;
echo "stage 6"
automake --add-missing --copy;
echo "stage 7"
autoconf || exit;
echo "stage 8"
automake || exit;

echo;
echo "Running ./configure ${CONFIGURE_ARGS} $@"
echo;
./configure ${CONFIGURE_ARGS} $@

