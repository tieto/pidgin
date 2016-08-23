#! /bin/sh
# Pidgin and Finch: The Pimpin' Penguin IM Clients That're Good for the Soul
# Copyright (C) 2003-2008 Gary Kramlich <grim@reaperworld.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

###############################################################################
# Usage
###############################################################################
# This script uses a config file that can be used to stash common arguments
# passed to configure or environment variables that need to be set before
# configure is called.  The configuration file is a simple shell script that
# gets sourced.
#
# By default, the config file that is used is named 'autogen.args'.  This can
# be configured below.
#
# Available options that are handled are as follow:
#   ACLOCAL_FLAGS - command line arguments to pass to aclocal
#   AUTOCONF_FLAGS - command line arguments to pass to autoconf
#   AUTOHEADER_FLAGS - command line arguments to pass to autoheader
#   AUTOMAKE_FLAGS - command line arguments to pass to automake flags
#   CONFIGURE_FLAGS - command line arguments to pass to configure
#   GLIB_GETTEXTIZE_FLAGS - command line arguments to pass to glib-gettextize
#   GTKDOCIZE_FLAGS - command line arguments to pass to gtkdocize
#   INTLTOOLIZE_FLAGS - command line arguments to pass to intltoolize
#   LIBTOOLIZE_FLAGS - command line arguments to pass to libtoolize
#
# Other helpful notes:
#   If you're using a different c compiler, you can override the environment
#   variable in 'autogen.args'.  For example, say you're using distcc, just add
#   the following to 'autogen.args':
#
#       CC="distcc"
#
#   This will work for any influential environment variable to configure.
###############################################################################
PACKAGE="Pidgin"
ARGS_FILE="autogen.args"
export CFLAGS
export LDFLAGS

DEFAULT_ACLOCAL_FLAGS="-I m4macros"

libtoolize="libtoolize"
case $(uname -s) in
	Darwin*)
		libtoolize="glibtoolize"
		if [ -d /usr/local/opt/gettext ] ; then
			PATH=/usr/local/opt/gettext/bin:$PATH
			DEFAULT_ACLOCAL_FLAGS="${DEFAULT_ACLOCAL_FLAGS} -I /usr/local/opt/gettext/share/aclocal"
		fi
		if [ -d /usr/local/opt/gobject-introspection ] ; then
			PATH=/usr/local/opt/gobject-introspection/bin:$PATH
			DEFAULT_ACLOCAL_FLAGS="${DEFAULT_ACLOCAL_FLAGS} -I /usr/local/opt/gobject-introspection/share/aclocal"
		fi
		;;
	*)
esac

###############################################################################
# Some helper functions
###############################################################################
check () {
	CMD=$1

	printf "%s" "checking for ${CMD}... "
	BIN=`which ${CMD} 2>/dev/null`

	if [ x"${BIN}" = x"" ] ; then
		echo "not found."
		echo "${CMD} is required to build ${PACKAGE}!"
		exit 1;
	fi

	echo "${BIN}"
}

run_or_die () { # beotch
	CMD=$1
	shift

	OUTPUT=`mktemp autogen-XXXXXX`

	printf "running %s %s... " ${CMD} "$*"
	${CMD} ${@} >${OUTPUT} 2>&1

	if [ $? != 0 ] ; then
		echo "failed."
		cat ${OUTPUT}
		rm -f ${OUTPUT}
		exit 1
	else
		echo "done."
		cat ${OUTPUT}

		rm -f ${OUTPUT}
	fi
}

check_gtkdoc() {
	printf "checking for gtkdocize... "
	GTKDOCIZE=`which gtkdocize 2>/dev/null`

	if [ x"${GTKDOCIZE}" = x"" ] ; then
		echo "not found."
		echo "EXTRA_DIST =" > gtk-doc.make
		echo "You don't have gtk-doc installed, and thus won't be able to
generate the documentation.
"
	else
		echo "${GTKDOCIZE}"
		run_or_die ${GTKDOCIZE} ${GTKDOCIZE_FLAGS}
	fi
}

cleanup () {
	rm -f autogen-??????
	echo
	exit 2
}

###############################################################################
# We really start here, yes, very sneaky!
###############################################################################
trap cleanup 2

FIGLET=`which figlet 2> /dev/null`
if [ x"${FIGLET}" != x"" ] ; then
	${FIGLET} -f small ${PACKAGE}
	echo "build system is being generated"
else
	echo "autogenerating build system for '${PACKAGE}'"
fi

###############################################################################
# Look for our args file
###############################################################################
printf "%s" "checking for ${ARGS_FILE}: "
if [ -f ${ARGS_FILE} ] ; then
	echo "found."
	printf "%s" "sourcing ${ARGS_FILE}: "
	. "`dirname "$0"`"/${ARGS_FILE}
	echo "done."
else
	echo "not found."
fi

###############################################################################
# Work inside the source directory
##############################################################################
test -z "$SRCDIR" && SRCDIR=`dirname "$0"`
test -z "$SRCDIR" && SRCDIR=.

OLDDIR=`pwd`
cd "$SRCDIR"

###############################################################################
# Check for our required helpers
###############################################################################
check "$libtoolize";		LIBTOOLIZE=${BIN};
check "glib-gettextize";	GLIB_GETTEXTIZE=${BIN};
check "intltoolize";		INTLTOOLIZE=${BIN};
check "sed";			SED=${BIN};
check "aclocal";		ACLOCAL=${BIN};
check "autoheader";		AUTOHEADER=${BIN};
check "automake";		AUTOMAKE=${BIN};
check "autoconf";		AUTOCONF=${BIN};

###############################################################################
# Run all of our helpers
###############################################################################
run_or_die ${LIBTOOLIZE} ${LIBTOOLIZE_FLAGS:-"-c -f --automake"}
run_or_die ${GLIB_GETTEXTIZE} ${GLIB_GETTEXTIZE_FLAGS:-"--force --copy"}
run_or_die ${INTLTOOLIZE} ${INTLTOOLIZE_FLAGS:-"-c -f --automake"}
# This call to sed is needed to work around an annoying bug in intltool 0.40.6
# See https://developer.pidgin.im/ticket/9520 for details
run_or_die ${SED} -i -e "s:'\^\$\$lang\$\$':\^\$\$lang\$\$:g" po/Makefile.in.in
# glib-gettextize doesn't seems to use AM_V_GEN macro
${SED} -i -e "s:\\tfile=\`echo:\\t@echo -e \"  GEN\\\\t\$\@\"; file=\`echo:g" po/Makefile.in.in
run_or_die ${ACLOCAL} ${ACLOCAL_FLAGS:-"${DEFAULT_ACLOCAL_FLAGS}"}
run_or_die ${AUTOHEADER} ${AUTOHEADER_FLAGS}
check_gtkdoc
run_or_die ${AUTOMAKE} ${AUTOMAKE_FLAGS:-"-a -c --gnu"}
run_or_die ${AUTOCONF} ${AUTOCONF_FLAGS}

###############################################################################
# Run configure
###############################################################################
cd "$OLDDIR"
if test -z "$NOCONFIGURE"; then
	echo "running $SRCDIR/configure ${CONFIGURE_FLAGS} $@"
	"$SRCDIR/configure" ${CONFIGURE_FLAGS} $@
fi
