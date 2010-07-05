dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.50])

# UPDATING VERSION NUMBERS FOR RELEASES
#
# gnt_micro_version += 1
#
# If any functions have been added to libgnt:
#   gnt_micro_version = 0
#   gnt_minor_version += 1
#   gnt_lt_current += 1
#
# If backwards compatibility has been broken in libgnt:
#   gnt_micro_version = 0
#   gnt_minor_version = 0
#   gnt_major_version += 1;
#   gnt_lt_current += 1
#
# gnt_version_suffix should be similar to one of the following:
#   For beta releases:          [beta2]
#   For code under development: [devel]
#   For production releases:    []
#
# Make sure to update ../../configure.ac with libgnt version changes.
#

m4_define([gnt_lt_current], [7])
m4_define([gnt_major_version], [2])
m4_define([gnt_minor_version], [7])
m4_define([gnt_micro_version], [0])
m4_define([gnt_version_suffix], [devel])
m4_define([gnt_version],
          [gnt_major_version.gnt_minor_version.gnt_micro_version])
m4_define([gnt_display_version], gnt_version[]m4_ifdef([gnt_version_suffix],[gnt_version_suffix]))

AC_INIT([libgnt], [gnt_display_version], [devel@pidgin.im])
AC_CANONICAL_SYSTEM
AM_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

GNT_MAJOR_VERSION=gnt_major_version
GNT_MINOR_VERSION=gnt_minor_version
GNT_MICRO_VERSION=gnt_micro_version
GNT_VERSION=[gnt_display_version]
AC_SUBST(GNT_MAJOR_VERSION)
AC_SUBST(GNT_MINOR_VERSION)
AC_SUBST(GNT_MICRO_VERSION)
AC_SUBST(GNT_VERSION)

GNT_LT_VERSION_INFO="gnt_lt_current:gnt_micro_version:gnt_minor_version"
AC_SUBST(GNT_LT_VERSION_INFO)

AC_PATH_PROG(sedpath, sed)

dnl Storing configure arguments
AC_DEFINE_UNQUOTED(CONFIG_ARGS, "$ac_configure_args", [configure arguments])

dnl Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AM_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL

dnl we don't use autobreak on cygwin!!
dnl AC_CYGWIN

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(arpa/nameser_compat.h fcntl.h sys/time.h unistd.h locale.h signal.h stdint.h regex.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_STRUCT_TM

AC_C_BIGENDIAN

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(strdup strstr atexit setlocale)

dnl to prevent the g_stat()/g_unlink() crash,
dnl (09:50:07) Robot101: LSchiere2: it's easy. +LC_SYS_LARGEFILE somewhere in configure.ac
AC_SYS_LARGEFILE

dnl FreeBSD doesn't have libdl, dlopen is provided by libc
AC_CHECK_FUNC(dlopen, LIBDL="", [AC_CHECK_LIB(dl, dlopen, LIBDL="-ldl")])

AC_MSG_CHECKING(for the %z format string in strftime())
AC_TRY_RUN([
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#include <stdio.h>

int main()
{
	char buf[6];
	time_t t = time(NULL);

	if (strftime(buf, sizeof(buf), "%z", localtime(&t)) != 5)
		return 1;

	fprintf(stderr, "strftime(\"%%z\") yields: \"%s\"\n", buf);

	return !((buf[0] == '-' || buf[0] == '+') &&
	         (buf[1] >= '0' && buf[1] <= '9') &&
	         (buf[2] >= '0' && buf[2] <= '9') &&
	         (buf[3] >= '0' && buf[3] <= '9') &&
	         (buf[4] >= '0' && buf[4] <= '9')
	        );
}
],
[
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_STRFTIME_Z_FORMAT], [1],
                                      [Define to 1 if you have a strftime() that supports the %z format string.])
],
[
	AC_MSG_RESULT(no)
],
[
	# Fallback for Cross Compiling...
	# This will enable the compatibility code.
	AC_MSG_RESULT(no)
]
)


AC_CHECK_HEADER(sys/utsname.h)
AC_CHECK_FUNC(uname)

if test "x$enable_debug" = "xyes" ; then
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
	enable_fatal_asserts="yes"
fi

if test "x$enable_fatal_asserts" = "xyes" ; then
	AC_DEFINE(GAIM_FATAL_ASSERTS, 1, [Define to make assertions fatal (useful for debugging).])
fi

if test "x$enable_deprecated" = "xno"; then
	DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
fi

if test "x$GCC" = "xyes"; then
	dnl We enable -Wall later.
	dnl If it's set after the warning CFLAGS in the compiler invocation, it counteracts the -Wno... flags.
	dnl This leads to warnings we don't want.
	CFLAGS=`echo $CFLAGS |$sedpath 's/-Wall//'`

	dnl ENABLE WARNINGS SUPPORTED BY THE VERSION OF GCC IN USE
	dnl
	dnl Future Possibilities
	dnl
	dnl Consider adding -Wbad-function-cast.
	dnl	This leads to spurious warnings using GPOINTER_TO_INT(), et al. directly on a function call.
	dnl		We'd need an intermediate variable.
	dnl
	dnl Consider adding -Wfloat-equal.
	dnl	This leads to warnings with Perl.
	dnl 		Perhaps we could write ugly configure magic and pass -Wno-float-equal down to that subdirectory.
	dnl		On the other hand, it's probably actually broken, so maybe the Perl folks should fix that?
	dnl
	dnl Consider removing -Wno-sign-compare (from the -Wextra set) and fixing all those cases.
	dnl	This is likely non-trivial.
	dnl
	for newflag in \
			"-Waggregate-return" \
			"-Wcast-align" \
			"-Wdeclaration-after-statement" \
			"-Werror-implicit-function-declaration" \
			"-Wextra -Wno-sign-compare -Wno-unused-parameter" \
			"-Winit-self" \
			"-Wmissing-declarations" \
			"-Wmissing-prototypes" \
			"-Wnested-externs" \
			"-Wpointer-arith" \
			"-Wundef" \
	; do
		orig_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $newflag"
		AC_MSG_CHECKING(for $newflag option to gcc)
		AC_TRY_COMPILE([], [
			int main() {return 0;}
		], [
			AC_MSG_RESULT(yes)
			CFLAGS="$orig_CFLAGS"
			DEBUG_CFLAGS="$DEBUG_CFLAGS $newflag"
		], [
			AC_MSG_RESULT(no)
			CFLAGS="$orig_CFLAGS"
		])
	done

	if test "x$enable_fortify" = "xyes"; then
		AC_MSG_CHECKING(for FORTIFY_SOURCE support)
		AC_TRY_COMPILE([#include <features.h>], [
			int main() {
			#if !(__GNUC_PREREQ (4, 1) \
				|| (defined __GNUC_RH_RELEASE__ && __GNUC_PREREQ (4, 0)) \
				|| (defined __GNUC_RH_RELEASE__ && __GNUC_PREREQ (3, 4) \
					&& __GNUC_MINOR__ == 4 \
					&& (__GNUC_PATCHLEVEL__ > 2 \
						|| (__GNUC_PATCHLEVEL__ == 2 && __GNUC_RH_RELEASE__ >= 8))))
			#error No FORTIFY_SOURCE support
			#endif
				return 0;
			}
		], [
			AC_MSG_RESULT(yes)
			DEBUG_CFLAGS="$DEBUG_CFLAGS -D_FORTIFY_SOURCE=2"
		], [
			AC_MSG_RESULT(no)
		])
	fi

	DEBUG_CFLAGS="-Wall $DEBUG_CFLAGS"
	CFLAGS="-g $CFLAGS"
fi
AC_SUBST(CFLAGS)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.0.0 gobject-2.0 gmodule-2.0],,
	[
		AC_MSG_ERROR([
*** GLib 2.0 is required to build LibGNT; please make sure you have the GLib
*** development headers installed. The latest version of GLib is
*** always available at http://www.gtk.org/.])
	])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


AC_MSG_CHECKING(for me pot o' gold)
AC_MSG_RESULT(no)
AC_CHECK_FUNCS(gethostid lrand48)
AC_CHECK_FUNCS(memcpy memmove random strchr strerror vprintf)
AC_CHECK_HEADERS(malloc.h paths.h sgtty.h stdarg.h sys/cdefs.h)
AC_CHECK_HEADERS(sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h)
AC_CHECK_HEADERS(sys/select.h sys/uio.h sys/utsname.h sys/wait.h)
AC_CHECK_HEADERS(termios.h)
#AC_CHECK_FUNC(wcwidth, [AC_DEFINE([HAVE_WCWIDTH], [1], [Define to 1 if you have wcwidth function.])])
#AC_VAR_TIMEZONE_EXTERNALS

GNT_CFLAGS=
GNT_LIBS=
AC_ARG_WITH(ncurses-headers, [AC_HELP_STRING([--with-ncurses-headers=DIR],
		[compile finch against the ncurses includes in DIR])],
		[ac_ncurses_includes="$withval"], [ac_ncurses_includes=""])
AC_CHECK_LIB(ncursesw, initscr, [GNT_LIBS="-lncursesw"], [enable_gnt=no])
AC_CHECK_LIB(panelw, update_panels, [GNT_LIBS="$GNT_LIBS -lpanelw"], [enable_gnt=no])

# If ncursesw is not found, look for plain old ncurses
if test "x$enable_gnt" = "xno"; then
	AC_CHECK_LIB(ncurses, initscr, [[GNT_LIBS="-lncurses"] [enable_gnt=yes]], [enable_gnt=no])
	AC_CHECK_LIB(panel, update_panels, [[GNT_LIBS="$GNT_LIBS -lpanel"] [enable_gnt=yes]], [enable_gnt=no])
	AC_DEFINE(NO_WIDECHAR, [1], [Define to 1 if you do not have ncursesw.])
else
	dnl # Some distros put the headers in ncursesw/, some don't
	found_ncurses_h=no
	for location in $ac_ncurses_includes /usr/include/ncursesw /usr/include
	do
		f="$location/ncurses.h"
		orig_CFLAGS="$CFLAGS"
		orig_CPPFLAGS="$CPPFLAGS"
		CFLAGS="$CFLAGS -I$location"
		CPPFLAGS="$CPPFLAGS -I$location"
		AC_CHECK_HEADER($f,[
			AC_MSG_CHECKING([if $f supports wide characters])
			AC_TRY_COMPILE([
				#define _XOPEN_SOURCE_EXTENDED
				#include <$f>
			], [
				#ifndef get_wch
				# error get_wch not found!
				#endif
			], [
				dir=$location
				if test x"$dir" != x"." ; then
					GNT_CFLAGS="-I$dir/"
				else
					GNT_CFLAGS=""
				fi

				found_ncurses_h=yes
				CFLAGS="$orig_CFLAGS"
				CPPFLAGS="$orig_CPPFLAGS"
				AC_MSG_RESULT([yes])
				break
			], [
				CFLAGS="$orig_CFLAGS"
				CPPFLAGS="$orig_CPPFLAGS"
				AC_MSG_RESULT([no])
			])
		])
	done
	if test x"$found_ncurses_h" != "xyes"; then
		enable_gnt="no"
	fi
fi
AC_SUBST(GNT_CFLAGS)
AC_SUBST(GNT_LIBS)

if test "x$enable_gnt" = "xno"; then
	AC_MSG_ERROR([
*** You need ncursesw or ncurses and its header files.])
fi

dnl Check for Python headers (currently useful only for libgnt)
dnl (Thanks to XChat)
AC_PATH_PROG(pythonpath, python)
if test "_$pythonpath" != _ ; then
	AC_MSG_CHECKING(for Python compile flags)
	PY_PREFIX=`$pythonpath -c 'import sys ; print sys.prefix'`
	PY_EXEC_PREFIX=`$pythonpath -c 'import sys ; print sys.exec_prefix'`
	changequote(<<, >>)dnl
	PY_VERSION=`$pythonpath -c 'import sys ; print sys.version[0:3]'`
	PY_MAJOR=`$pythonpath -c 'import sys ; print sys.version[0:2]'`
	changequote([, ])dnl
	if test -f $PY_PREFIX/include/python$PY_VERSION/Python.h -a "$PY_MAJOR" = "2."; then
		AC_CHECK_LIB(pthread, pthread_create, )
		AC_CHECK_LIB(util, openpty, )
		AC_CHECK_LIB(db, dbopen, )
		PY_LIBS="-lpython$PY_VERSION -L$PY_EXEC_PREFIX/lib/python$PY_VERSION/config"
		PY_CFLAGS="-I$PY_PREFIX/include/python$PY_VERSION"
		AC_DEFINE(USE_PYTHON, [1], [Define if python headers are available.])
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT([Can't find Python.h])
		PY_LIBS=""
		PY_CFLAGS=""
	fi
fi
AC_SUBST(PY_CFLAGS)
AC_SUBST(PY_LIBS)

dnl Check for libxml
have_libxml=yes
PKG_CHECK_MODULES(LIBXML, [libxml-2.0], , [
		AC_MSG_RESULT(no)
		have_libxml=no
	])
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

if test "x$have_libxml" = "xno"; then
	AC_DEFINE(NO_LIBXML, 1, [Do not have libxml2.])
fi

AM_CONDITIONAL(PURPLE_AVAILABLE, false)

AC_OUTPUT([Makefile
           gnt.pc
           wms/Makefile
		  ])

