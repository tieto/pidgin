dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.50])

# UPDATING VERSION NUMBERS FOR RELEASES
#
# purple_micro_version += 1
#
# If any functions have been added to libpurple, Pidgin, or Finch:
#   purple_micro_version = 0
#   purple_minor_version += 1
#   purple_lt_current += 1
#
# If backwards compatibility has been broken in libpurple, Pidgin, or Finch:
#   purple_micro_version = 0
#   purple_minor_version = 0
#   purple_major_version += 1;
#   purple_lt_current += 1
#
# purple_version_suffix should be similar to one of the following:
#   For beta releases:          [beta2]
#   For code under development: [devel]
#   For production releases:    []
#
#
# If any code has changed in libgnt:
#   gnt_micro_version += 1
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
# Make sure to update finch/libgnt/configure.ac with libgnt version changes.
#
m4_define([purple_lt_current], [7])
m4_define([purple_major_version], [2])
m4_define([purple_minor_version], [7])
m4_define([purple_micro_version], [6])
m4_define([purple_version_suffix], [devel])
m4_define([purple_version],
          [purple_major_version.purple_minor_version.purple_micro_version])
m4_define([purple_display_version], purple_version[]m4_ifdef([purple_version_suffix],[purple_version_suffix]))

m4_define([gnt_lt_current], [8])
m4_define([gnt_major_version], [2])
m4_define([gnt_minor_version], [8])
m4_define([gnt_micro_version], [3])
m4_define([gnt_version_suffix], [devel])
m4_define([gnt_version],
          [gnt_major_version.gnt_minor_version.gnt_micro_version])
m4_define([gnt_display_version], gnt_version[]m4_ifdef([gnt_version_suffix],[gnt_version_suffix]))


AC_INIT([pidgin], [purple_display_version], [devel@pidgin.im])

if test `pwd | wc -w` -ne 1; then
	AC_MSG_ERROR([
You are attempting to build in a path that contains spaces.  This
will fail.  Relocate this source tree to a path that does not contain
spaces and run configure again.
])
fi

AC_CANONICAL_HOST
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.9 -Wno-portability dist-bzip2])
dnl TODO: Always use AM_SILENT_RULES when we depend on automake >= 1.11
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

PURPLE_MAJOR_VERSION=purple_major_version
PURPLE_MINOR_VERSION=purple_minor_version
PURPLE_MICRO_VERSION=purple_micro_version
PURPLE_VERSION=[purple_display_version]
AC_SUBST(PURPLE_MAJOR_VERSION)
AC_SUBST(PURPLE_MINOR_VERSION)
AC_SUBST(PURPLE_MICRO_VERSION)
AC_SUBST(PURPLE_VERSION)

PURPLE_LT_VERSION_INFO="purple_lt_current:purple_micro_version:purple_minor_version"
AC_SUBST(PURPLE_LT_VERSION_INFO)

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
AM_PROG_CC_C_O
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG
AC_FUNC_ALLOCA

dnl Check for Sun compiler
AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(arpa/nameser_compat.h fcntl.h sys/time.h unistd.h locale.h signal.h stdint.h regex.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_STRUCT_TM
AC_CHECK_SIZEOF(time_t, ,[
#include <stdio.h>
#include <time.h>])

AC_C_BIGENDIAN

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(strdup strstr atexit setlocale)
dnl Checks for getopt in standard library
AC_CHECK_FUNCS(getopt_long,,
[
	AC_LIBOBJ(getopt)
	AC_LIBOBJ(getopt1)
])

dnl Check for inet_aton
AC_CHECK_FUNC(inet_aton, , [AC_CHECK_LIB(resolv, inet_aton, ,
				         [AC_MSG_ERROR([inet_aton not found])])])
AC_CHECK_LIB(resolv, __res_query)
AC_CHECK_LIB(nsl, gethostent)
AC_CHECK_FUNC(socket, ,
	[AC_CHECK_LIB(socket, socket, , [AC_MSG_ERROR([socket not found])])])
dnl If all goes well, by this point the previous two checks will have
dnl pulled in -lsocket and -lnsl if we need them.
AC_CHECK_FUNC(getaddrinfo,
	[AC_DEFINE([HAVE_GETADDRINFO], [1],
		[Define to 1 if you have the getaddrinfo function.])],
	[AC_CHECK_LIB(socket, getaddrinfo,
		[AC_DEFINE([HAVE_GETADDRINFO]) LIBS="-lsocket -lsnl $LIBS"], , , -lnsl)])
AC_CHECK_FUNCS(inet_ntop)
AC_CHECK_FUNCS(getifaddrs)
dnl Check for socklen_t (in Unix98)
AC_MSG_CHECKING(for socklen_t)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#include <sys/types.h>
	#include <sys/socket.h>
	socklen_t x;
]], [[]])], [
	AC_MSG_RESULT(yes)
], [
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#include <sys/types.h>
		#include <sys/socket.h>
		int accept(int, struct sockaddr *, size_t *);
	]], [[]])], [
		AC_MSG_RESULT(size_t)
		AC_DEFINE(socklen_t, size_t, [socklen_t size])
	], [
		AC_MSG_RESULT(int)
		AC_DEFINE(socklen_t, int, [socklen_t size])
	])
])

dnl Some systems do not have sa_len field for struct sockaddr.
AC_CHECK_MEMBER([struct sockaddr.sa_len],
	[AC_DEFINE([HAVE_STRUCT_SOCKADDR_SA_LEN],[1],
	[Define if struct sockaddr has an sa_len member])],[:],
	[#include <sys/socket.h>])

dnl Check for v6-only sockets
AC_CHECK_DECL([IPV6_V6ONLY],
	[AC_DEFINE([HAVE_IPV6_V6ONLY],[1],
	[Define if the IPV6_V6ONLY setsockopt option exists])],
	[], [#include <netinet/in.h>])

dnl to prevent the g_stat()/g_unlink() crash,
dnl (09:50:07) Robot101: LSchiere2: it's easy. +LC_SYS_LARGEFILE somewhere in configure.ac
AC_SYS_LARGEFILE

dnl FreeBSD doesn't have libdl, dlopen is provided by libc
AC_CHECK_FUNC(dlopen, LIBDL="", [AC_CHECK_LIB(dl, dlopen, LIBDL="-ldl")])

AC_MSG_CHECKING(for fileno())
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>

int main(int argc, char *argv[])
{
	int fd;

	fd = fileno(stdout);

	return !(fd > 0);
}
]])], [
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_FILENO], [1],
	          [Define to 1 if your stdio has int fileno(FILE *).])
], [
	AC_MSG_RESULT(no)
], [
	# Fallback for Cross Compiling...
	# This will enable the compatibility code.
	AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for the %z format string in strftime())
AC_RUN_IFELSE([AC_LANG_SOURCE([[
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
]])], [
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_STRFTIME_Z_FORMAT], [1],
		[Define to 1 if you have a strftime() that supports the %z format string.])
], [
	AC_MSG_RESULT(no)
], [
	# Fallback for Cross Compiling...
	# This will enable the compatibility code.
	AC_MSG_RESULT(no)
])

dnl #######################################################################
dnl # Disable creation and installation of translation files
dnl #######################################################################
AC_ARG_ENABLE(nls, AC_HELP_STRING([--disable-nls], [disable installation of translation files]), enable_i18n="$enableval", enable_i18n=yes)

if test x$enable_i18n = xyes; then
	AC_PROG_INTLTOOL
	GETTEXT_PACKAGE=pidgin
	AC_SUBST(GETTEXT_PACKAGE)


	# before gettexting, in case iconv matters
	case "$host_os" in
	darwin*)
		AC_CHECK_LIB(resolv, res_query)

		AC_CHECK_HEADER(CoreFoundation/CoreFoundation.h, [
			AC_CHECK_HEADER(IOKit/IOKitLib.h, [
				AC_DEFINE(HAVE_IOKIT, 1, [Define if we have IOKit])
				LIBS="$LIBS -framework IOKit -framework CoreFoundation"
			], [])
		], [])

		AC_MSG_CHECKING([for fink])
		if test -d /sw; then
			AC_MSG_RESULT([found, adding /sw to search paths])
			CPPFLAGS="$CPPFLAGS -I/sw/include"
			LDFLAGS="$LDFLAGS -L/sw/lib"
		else
			AC_MSG_RESULT([not found])
		fi
		;;
	*)
		;;
	esac

	ALL_LINGUAS="af am ar az be@latin bg bn bn_IN bs ca ca@valencia cs da de dz el en_AU en_CA en_GB eo es et eu fa fi fr ga gl gu he hi hu hy id it ja ka km kn ko ku lo lt mai mk mn mr ms_MY my_MM nb ne nl nn oc or pa pl pt_BR pt ps ro ru si sk sl sq sr sr@latin sv sw ta te th tr uk ur vi xh zh_CN zh_HK zh_TW"
	AM_GLIB_GNU_GETTEXT

	dnl If we don't have msgfmt, then po/ is going to fail -- ensure that
	dnl AM_GLIB_GNU_GETTEXT found it.

	if test x$MSGFMT = xno -o x$MSGFMT$GMSGFMT$INTLTOOL_MSGFMT = x
	then
		AC_MSG_ERROR([

The msgfmt command is required to build libpurple.  If it is installed
on your system, ensure that it is in your path.  If it is not, install
GNU gettext to continue.

If you have msgfmt installed, but for some reason this error message
is still displayed, you have encountered what appears to be a bug in
third-party configure macros.  Try setting the MSGFMT environment
variable to the absolute path to your msgfmt binary and trying
configure again, like this:

MSGFMT=/path/to/msgfmt ./configure ...
	])
	fi
fi #enable_i18n

AM_CONDITIONAL(INSTALL_I18N, test "x$enable_i18n" = "xyes")

dnl #######################################################################
dnl # Check for GLib 2.12 (required)
dnl #######################################################################
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.12.0 gobject-2.0 gmodule-2.0 gthread-2.0], , [
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([

You must have GLib 2.12.0 or newer development headers installed to build.

If you have these installed already you may need to install pkg-config so
I can find them.
])])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

AC_ARG_WITH([extraversion],
			AC_HELP_STRING([--with-extraversion=STRING],
						   [extra version number to be displayed in Help->About and --help (for packagers)]),
						   EXTRA_VERSION=$withval)

if test x"$EXTRA_VERSION" != "x" ; then
	AC_DEFINE_UNQUOTED(DISPLAY_VERSION, "$VERSION-$EXTRA_VERSION", [display version info])
else
	AC_DEFINE_UNQUOTED(DISPLAY_VERSION, "$VERSION", [display version info])
fi

AC_ARG_ENABLE(missing-dependencies, [AC_HELP_STRING([--disable-missing-dependencies],
		[skip missing dependencies instead of aborting configure])],
	force_deps="$enableval", force_deps="yes")

AC_ARG_WITH(x, [],
	with_x="$withval", with_x="yes")
AC_ARG_ENABLE(gtkui, [AC_HELP_STRING([--disable-gtkui],
		[compile without GTK+ user interface])],
	enable_gtkui="$enableval", enable_gtkui="yes")
AC_ARG_ENABLE(consoleui, [AC_HELP_STRING([--disable-consoleui],
		[compile without console user interface])],
	[enable_consoleui=$enableval force_finch=$enableval], [enable_consoleui=yes force_finch=no])

dnl #######################################################################
dnl # Check for GTK+ 2.10 and other things used by the GTK UI
dnl #######################################################################
AC_ARG_ENABLE(screensaver,
	[AC_HELP_STRING([--disable-screensaver],
		[compile without X screensaver extension (used to detect idleness)])],
	enable_screensaver="$enableval", enable_screensaver="yes")
AC_ARG_ENABLE(sm,
	[AC_HELP_STRING([--disable-sm],
		[compile without X session management support])],
	enable_sm="$enableval", enable_sm="yes")
AC_ARG_ENABLE(startup-notification,
	[AC_HELP_STRING([--disable-startup-notification],
		[compile without startup notification support])],
	enable_startup_notification="$enableval", enable_startup_notification="yes")
AC_ARG_ENABLE(gtkspell,
	[AC_HELP_STRING([--disable-gtkspell],
		[compile without GtkSpell automatic spell checking])],
	enable_gtkspell="$enableval", enable_gtkspell="yes")
AC_ARG_ENABLE(gevolution,
	[AC_HELP_STRING([--enable-gevolution],
		[compile with the Evolution plugin])],
	enable_gevolution="$enableval", enable_gevolution="no")
AC_ARG_ENABLE(cap,
	[AC_HELP_STRING([--enable-cap],
		[compile with Contact Availability Prediction plugin])],
	enable_cap="$enableval", enable_cap="no")
AC_ARG_ENABLE(gestures,
	[AC_HELP_STRING([--disable-gestures],
		[compile without the gestures plugin])],
	enable_gestures="$enableval", enable_gestures="yes")

AC_PATH_XTRA
# We can't assume that $x_libraries will be set, because autoconf does not
# set it in the case when the X libraries are in a standard place.
# Ditto for $x_includes
if test X"$x_libraries" = X"" || test X"$x_libraries" = XNONE; then
    x_libpath_add=
else
    x_libpath_add="-L$x_libraries"
fi
if test X"$x_includes" = X"" || test X"$x_includes" = XNONE; then
    x_incpath_add=
else
    x_incpath_add="-I$x_includes"
fi

if test "x$enable_gtkui" = "xyes" ; then
	PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= 2.91.6], , [
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([

You must have GTK+ 2.91.5 or newer development headers installed to compile
Pidgin.  If you want to build only Finch then specify --disable-gtkui when
running configure.
])])

	AC_SUBST(GTK_CFLAGS)
	AC_SUBST(GTK_LIBS)

	dnl We only really need Pango >= 1.4 for decent RTL support
	PKG_CHECK_MODULES(PANGO, [pango >= 1.4.0],
			AC_DEFINE(HAVE_PANGO14, 1, [Define if we have Pango 1.4 or newer.]),:)

	dnl #######################################################################
	dnl # Check if we should compile with X support
	dnl #######################################################################
	if test "x$with_x" = "xyes" ; then
		PKG_CHECK_MODULES(X11, x11,
			[AC_DEFINE(HAVE_X11, 1, [Define to 1 if you have X11])],
			[AC_MSG_RESULT(no)
				if test "x$x_incpath_add" != "x" -a "x$x_libpath_add" != "x"; then
					X11_LIBS="$x_libpath_add"
					X11_CFLAGS="$x_incpath_add"
				else
					with_x="no"
					if test "x$force_deps" = "xyes" ; then
						AC_MSG_ERROR([
X11 development headers not found.
Use --without-x if you do not need X11 support.
])
					fi
				fi
			])
		AC_SUBST(X11_LIBS)
		AC_SUBST(X11_CFLAGS)
	else
		enable_screensaver=no
		enable_sm=no
		enable_gestures=no
	fi

	dnl #######################################################################
	dnl # Check for XScreenSaver
	dnl #######################################################################
	if test "x$enable_screensaver" = "xyes" ; then
		if test "x$with_x" = "xyes" ; then
			old_LIBS="$LIBS"
			LIBS="$LIBS $GTK_LIBS $x_libpath_add"
			XSS_LIBS=""
			XSS_HEADERS=""
			AC_CHECK_LIB(Xext, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_EXTRA_LIBS"],[],[-lX11 -lXext -lm])
			AC_CHECK_LIB(Xss, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_LIBS $X_EXTRA_LIBS -lXss"],[],[-lX11 -lXext -lm])
			if test "x$XSS_LIBS" != "x"; then
				oldCPPFLAGS="$CPPFLAGS"
				CPPFLAGS="$CPPFLAGS $x_incpath_add"
				AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
					#include <X11/Xlib.h>
					#include <X11/extensions/scrnsaver.h>
					]], [[]])], [], [enable_screensaver=no])
				CPPFLAGS="$oldCPPFLAGS"
			else
				enable_screensaver=no
			fi
			LIBS="$old_LIBS"

			if test "x$enable_screensaver" = "xyes" ; then
				AC_DEFINE(USE_SCREENSAVER, 1, [Define if we're using XScreenSaver.])
				AC_SUBST(XSS_LIBS)
			else
				if test "x$force_deps" = "xyes" ; then
					AC_MSG_ERROR([
XScreenSaver extension development headers not found.
Use --disable-screensaver if you do not need XScreenSaver extension support,
this is required for detecting idle time by mouse and keyboard usage.
])
				fi
			fi
		else
			AC_MSG_ERROR([X support is required to build with XScreenSaver extensions])
		fi
	fi

	dnl #######################################################################
	dnl # Check for X session management libs
	dnl #######################################################################
	if test "x$enable_sm" = "xyes"; then
		if test "x$with_x" = "xyes" ; then
			enable_sm=no
			AC_CHECK_LIB(SM, SmcSaveYourselfDone, found_sm_lib=true, , [$x_libpath_add -lICE])
			if test "x$found_sm_lib" = "xtrue"; then
				oldCPPFLAGS="$CPPFLAGS"
				CPPFLAGS="$CPPFLAGS $x_incpath_add"
				AC_CHECK_HEADERS(X11/SM/SMlib.h, SM_LIBS="$x_libpath_add -lSM -lICE" enable_sm=yes)
				CPPFLAGS="$oldCPPFLAGS"
			fi

			if test "x$enable_sm" = "xyes"; then
				AC_DEFINE(USE_SM, 1, [Define if we're using X Session Management.])
				AC_SUBST(SM_LIBS)
			else
				if test "x$force_deps" = "xyes" ; then
					AC_MSG_ERROR([
X session management development headers not found.
Use --disable-sm if you do not need session management support.
])
				fi
			fi
		else
			AC_MSG_ERROR([X support is required to build with X session management support])
		fi
	fi

	dnl #######################################################################
	dnl # Check for X11 to allow the gestures plugin
	dnl #######################################################################
	if test "x$enable_gestures" = "xyes"; then
		if test "x$with_x" = "xno" ; then
			enable_gestures=no
		fi
	fi

	dnl #######################################################################
	dnl # Check for startup notification
	dnl #######################################################################
	if test "x$enable_startup_notification" = "xyes"; then
		PKG_CHECK_MODULES(STARTUP_NOTIFICATION, [libstartup-notification-1.0 >= 0.5], , [
			AC_MSG_RESULT(no)
			enable_startup_notification="no"
			if test "x$force_deps" = "xyes" ; then
				AC_MSG_ERROR([
Startup notification development headers not found.
Use --disable-startup-notification if you do not need it.
])
			fi])

		if test "x$enable_startup_notification" = "xyes"; then
			AC_DEFINE(HAVE_STARTUP_NOTIFICATION, 1, [Define if we're using libstartup-notification.])
			AC_SUBST(STARTUP_NOTIFICATION_CFLAGS)
			AC_SUBST(STARTUP_NOTIFICATION_LIBS)
		fi
	fi

	dnl #######################################################################
	dnl # Check for GtkSpell
	dnl #######################################################################
	if test "x$enable_gtkspell" = "xyes" ; then
		PKG_CHECK_MODULES(GTKSPELL, gtkspell-2.0 >= 2.0.2, , [
			AC_MSG_RESULT(no)
			enable_gtkspell="no"
			if test "x$force_deps" = "xyes" ; then
				AC_MSG_ERROR([
GtkSpell development headers not found.
Use --disable-gtkspell if you do not need it.
])
			fi])
		if test "x$enable_gtkspell" = "xyes" ; then
			AC_DEFINE(USE_GTKSPELL, 1, [Define if we're using GtkSpell])
			AC_SUBST(GTKSPELL_CFLAGS)
			AC_SUBST(GTKSPELL_LIBS)
		fi
	fi

	dnl #######################################################################
	dnl # Check for stuff needed by the Evolution integration plugin.
	dnl #######################################################################
	if test "x$enable_gevolution" = "xyes"; then
		evo_deps="libebook-1.2 libedata-book-1.2"
		PKG_CHECK_MODULES(EVOLUTION_ADDRESSBOOK, $evo_deps, , [
			AC_MSG_RESULT(yes)
			enable_gevolution="no"
		])
		if test "x$enable_gevolution" = "xno"; then
			evo_deps="libebook-1.0 libedata-book-1.0"
			PKG_CHECK_MODULES(EVOLUTION_ADDRESSBOOK, $evo_deps, [
				enable_gevolution="yes"
			], [
				AC_MSG_RESULT(yes)
			])
		fi
		if test "x$enable_gevolution" = "xyes"; then
			AC_DEFINE(HAVE_EVOLUTION_ADDRESSBOOK, 1, [Define if we're using evolution addressbook.])
			AC_SUBST(EVOLUTION_ADDRESSBOOK_CFLAGS)
			AC_SUBST(EVOLUTION_ADDRESSBOOK_LIBS)
		else
			if test "x$force_deps" = "xyes" ; then
				AC_MSG_ERROR([
Evolution development headers not found.
Use --disable-gevolution if you do not need it.
])
			fi
		fi
	fi

	dnl #######################################################################
	dnl # Check for libsqlite3 (for the Contact Availability Prediction plugin)
	dnl #######################################################################
	if test "x$enable_cap" = "xyes"; then
		PKG_CHECK_MODULES(SQLITE3, sqlite3 >= 3.3,,[
			AC_MSG_RESULT(no)
			enable_cap="no"
			if test "x$force_deps" = "xyes" ; then
				AC_MSG_ERROR([
sqlite3 development headers not found.
Use --disable-cap if you do not need the Contact Availability Prediction plugin.
])
			fi])
	fi
        

else # GTK
	enable_cap=no
	enable_gevolution=no
	enable_gtkspell=no
	enable_screensaver=no
	enable_sm=no
	enable_startup_notification=no
fi	# GTK

AM_CONDITIONAL(ENABLE_GTK, test "x$enable_gtkui" = "xyes")
AM_CONDITIONAL(BUILD_GEVOLUTION, test "x$enable_gevolution" = "xyes")
AM_CONDITIONAL(ENABLE_CAP, test "x$enable_cap" = "xyes")
AM_CONDITIONAL(ENABLE_GESTURES, test "x$enable_gestures" = "xyes")


dnl #######################################################################
dnl # Check for ncurses and other things used by the console UI
dnl #######################################################################
GNT_LIBS=""
GNT_CFLAGS=""
AC_ARG_WITH(ncurses-headers, [AC_HELP_STRING([--with-ncurses-headers=DIR],
		[compile finch against the ncurses includes in DIR])],
		[ac_ncurses_includes="$withval"], [ac_ncurses_includes=""])
if test "x$enable_consoleui" = "xyes"; then
	AC_CHECK_LIB(ncursesw, initscr, [GNT_LIBS="-lncursesw"], [enable_consoleui=no])
	AC_CHECK_LIB(panelw, update_panels, [GNT_LIBS="$GNT_LIBS -lpanelw"],
	    [enable_consoleui=no], [$GNT_LIBS])

	if test "x$enable_consoleui" = "xyes"; then
		dnl # Some distros put the headers in ncursesw/, some don't
		found_ncurses_h=no
		for location in $ac_ncurses_includes $NCURSES_HEADERS /usr/include/ncursesw /usr/include
		do
			f="$location/ncurses.h"
			orig_CFLAGS="$CFLAGS"
			orig_CPPFLAGS="$CPPFLAGS"
			CFLAGS="$CFLAGS -I$location"
			CPPFLAGS="$CPPFLAGS -I$location"
			AC_CHECK_HEADER($f,[
				AC_MSG_CHECKING([if $f supports wide characters])
				AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
					#define _XOPEN_SOURCE_EXTENDED
					#include <$f>
				]], [[
					#ifndef get_wch
					# error get_wch not found!
					#endif
				]])], [
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

		if test x"$found_ncurses_h" = x"no" ; then
			GNT_LIBS=""
			GNT_CFLAGS=""
			enable_consoleui=no
		fi
	else
		# ncursesw was not found. Look for plain old ncurses
		enable_consoleui=yes
		AC_CHECK_LIB(ncurses, initscr, [GNT_LIBS="-lncurses"], [enable_consoleui=no])
		AC_CHECK_LIB(panel, update_panels, [GNT_LIBS="$GNT_LIBS -lpanel"],
		    [enable_consoleui=no], [$GNT_LIBS])
		AC_DEFINE(NO_WIDECHAR, 1, [Define to 1 if you don't have wide-character support.])
		if test x"$ac_ncurses_includes" != "x"; then
			GNT_CFLAGS="-I$ac_ncurses_includes"
		else
			if test x"$NCURSES_HEADERS" != "x"; then
				GNT_CFLAGS="-I$NCURSES_HEADERS"
			fi
		fi
	fi
fi

if test "x$force_finch" = "xyes" -a "x$enable_consoleui" != "xyes"; then
	AC_MSG_ERROR([

Finch will not be built. You need to install ncursesw (or ncurses) and its development headers.

])
fi

AC_SUBST(GNT_LIBS)
AC_SUBST(GNT_CFLAGS)
AM_CONDITIONAL(ENABLE_GNT, test "x$enable_consoleui" = "xyes")

#AC_CHECK_FUNC(wcwidth, [AC_DEFINE([HAVE_WCWIDTH], [1], [Define to 1 if you have wcwidth function.])])

dnl #######################################################################
dnl # Check for LibXML2 (required)
dnl #######################################################################
PKG_CHECK_MODULES(LIBXML, [libxml-2.0 >= 2.6.0], , [
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([
You must have libxml2 >= 2.6.0 development headers installed to build.
])])
PKG_CHECK_EXISTS([libxml-2.0 >= 2.6.18], , [
	AC_MSG_WARN([
Versions of libxml2 < 2.6.18 may contain bugs that could cause XMPP messages to be discarded.
])])

AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

dnl #######################################################################
dnl # GConf schemas
dnl #######################################################################
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AM_CONDITIONAL(USE_GCONFTOOL, test "x$GCONFTOOL" != "xno")
AM_GCONF_SOURCE_2

dnl #######################################################################
dnl # Check for GStreamer
dnl #######################################################################
dnl
dnl TODO: Depend on gstreamer >= 0.10.10, and remove the conditional use of
dnl       gst_registry_fork_set_enabled.
AC_ARG_ENABLE(gstreamer,
	[AC_HELP_STRING([--disable-gstreamer], [compile without GStreamer audio support])],
	enable_gst="$enableval", enable_gst="yes")
if test "x$enable_gst" != "xno"; then
	PKG_CHECK_MODULES(GSTREAMER, [gstreamer-0.10], [
		AC_DEFINE(USE_GSTREAMER, 1, [Use GStreamer for playing sounds])
		AC_SUBST(GSTREAMER_CFLAGS)
		AC_SUBST(GSTREAMER_LIBS)
		AC_CHECK_LIB(gstreamer-0.10, gst_registry_fork_set_enabled,
			[ AC_DEFINE(GST_CAN_DISABLE_FORKING, [],
			  [Define if gst_registry_fork_set_enabled exists])],
			[], [$GSTREAMER_LIBS])
	], [
		AC_MSG_RESULT(no)
		enable_gst="no"
		if test "x$force_deps" = "xyes" ; then
			AC_MSG_ERROR([
GStreamer development headers not found.
Use --disable-gstreamer if you do not need GStreamer (sound) support.
])
		fi])
fi

dnl #######################################################################
dnl # Check for GStreamer Interfaces
dnl #######################################################################
if test "x$enable_gst" != "xno"; then
	AC_ARG_ENABLE(gstreamer-interfaces,
		[AC_HELP_STRING([--disable-gstreamer-interfaces], [compile without GStreamer interface support])],
			enable_gstinterfaces="$enableval", enable_gstinterfaces="yes")
	if test "x$enable_gstinterfaces" != "xno"; then
		PKG_CHECK_MODULES(GSTINTERFACES, [gstreamer-interfaces-0.10], [
			AC_DEFINE(USE_GSTINTERFACES, 1, [Use GStreamer interfaces for X overlay support])
			AC_SUBST(GSTINTERFACES_CFLAGS)
			AC_SUBST(GSTINTERFACES_LIBS)
		], [
			enable_gstinterfaces="no"
		])
	fi
else
	enable_gstinterfaces="no"
fi

dnl #######################################################################
dnl # Check for Farsight
dnl #######################################################################
AC_ARG_ENABLE(farsight,
	[AC_HELP_STRING([--disable-farsight], [compile without farsight support])],
	enable_farsight="$enableval", enable_farsight="yes")
if test "x$enable_farsight" != "xno"; then
	PKG_CHECK_MODULES(FARSIGHT, [farsight2-0.10 >= 0.0.9], [
		AC_DEFINE(USE_FARSIGHT, 1, [Use Farsight for voice and video])
		AC_SUBST(FARSIGHT_CFLAGS)
		AC_SUBST(FARSIGHT_LIBS)
	], [
		enable_farsight="no"
	])
fi

dnl #######################################################################
dnl # Check for Voice and Video support
dnl #######################################################################
AC_ARG_ENABLE(vv,
	[AC_HELP_STRING([--disable-vv], [compile without voice and video support])],
	enable_vv="$enableval", enable_vv="yes")
if test "x$enable_vv" != "xno"; then
	if test "x$enable_gstreamer" != "xno" -a "x$enable_gstinterfaces" != "xno" -a "x$enable_farsight" != "xno"; then
		AC_DEFINE(USE_VV, 1, [Use voice and video])
	else
		enable_vv="no"
		if test "x$force_deps" = "xyes"; then
			AC_MSG_ERROR([
Dependencies for voice/video were not met.
Install the necessary gstreamer and farsight packages first.
Or use --disable-vv if you do not need voice/video support.
			])
		fi
	fi
fi
AM_CONDITIONAL(USE_VV, test "x$enable_gstreamer" != "xno" -a "x$enable_gstinterfaces" != "xno" -a "x$enable_farsight" != "xno")

dnl #######################################################################
dnl # Check for Internationalized Domain Name support
dnl #######################################################################

AC_ARG_ENABLE(idn,
	[AC_HELP_STRING([--disable-idn], [compile without IDN support])],
	[enable_idn="$enableval" force_idn=$enableval], [enable_idn="yes" force_idn=no])
if test "x$enable_idn" != "xno"; then
	PKG_CHECK_MODULES(IDN, libidn >= 0.0.0, [
		AC_DEFINE(USE_IDN, 1, [Use GNU Libidn for stringprep and IDN])
		AC_SUBST(IDN_CFLAGS)
		AC_SUBST(IDN_LIBS)
	], [
		AC_MSG_RESULT(no)
		enable_idn="no"
		if test "x$force_deps" = "xyes" ; then
			AC_MSG_ERROR([
GNU Libidn development headers not found.
Use --disable-idn if you do not need it.
])
		fi
	])
fi

dnl #######################################################################
dnl # Check for Meanwhile headers (for Sametime)
dnl #######################################################################
AC_ARG_ENABLE(meanwhile,
	[AC_HELP_STRING([--disable-meanwhile],
		[compile without meanwhile (required for Sametime support)])],
	enable_meanwhile="$enableval", enable_meanwhile="yes")
if test "x$enable_meanwhile" = "xyes"; then
	PKG_CHECK_MODULES(MEANWHILE, [meanwhile >= 1.0.0 meanwhile < 2.0.0], [
		have_meanwhile="yes"
	], [
		have_meanwhile="no"
		if test "x$force_deps" = "xyes" ; then
			AC_MSG_ERROR([
Meanwhile development headers not found.
Use --disable-meanwhile if you do not need meanwhile (Sametime) support.
])
		fi])
fi
AC_SUBST(MEANWHILE_CFLAGS)
AC_SUBST(MEANWHILE_LIBS)

dnl #######################################################################
dnl # Check for Native Avahi headers (for Bonjour)
dnl #######################################################################
AC_ARG_ENABLE(avahi,
	[AC_HELP_STRING([--disable-avahi],
		[compile without avahi (required for Bonjour support)])],
	enable_avahi="$enableval", enable_avahi="yes")

if test "x$enable_avahi" = "xyes"; then
	AC_ARG_WITH(avahi-client-includes, [AC_HELP_STRING([--with-avahi-client-includes=DIR], [compile the Bonjour plugin against the Avahi Client includes in DIR])], [ac_avahi_client_includes="$withval"], [ac_avahi_client_includes="no"])
	AC_ARG_WITH(avahi-client-libs, [AC_HELP_STRING([--with-avahi-client-libs=DIR], [compile the Bonjour plugin against the Avahi Client libs in DIR])], [ac_avahi_client_libs="$withval"], [ac_avahi_client_libs="no"])
	AVAHI_CFLAGS=""
	AVAHI_LIBS=""

	dnl Attempt to autodetect Avahi
	PKG_CHECK_MODULES(AVAHI, [avahi-client avahi-glib], [
		avahiincludes="yes"
		avahilibs="yes"
	], [
		avahiincludes="no"
		avahilibs="no"
	])

	dnl Override AVAHI_CFLAGS if the user specified an include dir
	if test "$ac_avahi_client_includes" != "no"; then
		AVAHI_CFLAGS="-I$ac_avahi_client_includes"
	fi
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $AVAHI_CFLAGS"
	AC_CHECK_HEADER(avahi-client/client.h, [avahiincludes=yes], [avahiincludes=no])
	CPPFLAGS="$CPPFLAGS $AVAHI_CFLAGS $GLIB_CFLAGS"
	AC_CHECK_HEADER(avahi-glib/glib-malloc.h, [avahiincludes=yes], [avahiincludes=no])
	CPPFLAGS="$CPPFLAGS_save"

	dnl Override AVAHI_LIBS if the user specified a libs dir
	if test "$ac_avahi_client_libs" != "no"; then
		AVAHI_LIBS="-L$ac_avahi_client_libs -lavahi-common -lavahi-client -lavahi-glib "
	fi
	AC_CHECK_LIB(avahi-client, avahi_client_new, [avahilibs=yes], [avahilibs=no], $AVAHI_LIBS)
fi

if test "x$enable_avahi" = "xyes" -a \( "x$avahiincludes" = "xno" -o "x$avahilibs" = "xno" \); then
	enable_avahi="no"
	if test "x$force_deps" = "xyes"; then
		AC_MSG_ERROR([
avahi development headers not found.
Use --disable-avahi if you do not need avahi (Bonjour) support.
])
	fi
fi
AC_SUBST(AVAHI_CFLAGS)
AC_SUBST(AVAHI_LIBS)


dnl #######################################################################
dnl # Check for SILC client includes and libraries
dnl #######################################################################
AC_ARG_WITH(silc-includes, [AC_HELP_STRING([--with-silc-includes=DIR], [compile the SILC plugin against includes in DIR])], [ac_silc_includes="$withval"], [ac_silc_includes="no"])
AC_ARG_WITH(silc-libs, [AC_HELP_STRING([--with-silc-libs=DIR], [compile the SILC plugin against the SILC libs in DIR])], [ac_silc_libs="$withval"], [ac_silc_libs="no"])
SILC_CFLAGS=""
SILC_LIBS=""
have_silc="no"
if test -n "$with_silc_includes" || test -n "$with_silc_libs"; then
	silc_manual_check="yes"
else
	silc_manual_check="no"
fi
if test "x$silc_manual_check" = "xno"; then
	PKG_CHECK_MODULES(SILC, [silcclient >= 1.1], [
		have_silc="yes"
		silcincludes="yes"
		silcclient="yes"
	], [
		have_silc="no"
	])
	if test "x$have_silc" = "xno"; then
		PKG_CHECK_MODULES(SILC, silcclient, [
			have_silc="yes"
			silc10includes="yes"
			silc10client="yes"
		], [
			have_silc="no"
		])
		dnl If silcclient.pc wasn't found, check for just silc.pc
		if test "x$have_silc" = "xno"; then
			PKG_CHECK_MODULES(SILC, silc, [
				have_silc="yes"
				silc10includes="yes"
				silc10client="yes"
			], [
				have_silc="no"
			])
		fi
	fi
else
	if test "$ac_silc_includes" != "no"; then
		SILC_CFLAGS="-I$ac_silc_includes"
	fi
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SILC_CFLAGS"
	AC_CHECK_HEADER(silc.h, [silcincludes=yes])
	CPPFLAGS="$CPPFLAGS_save"

	if test "$ac_silc_libs" != "no"; then
		SILC_LIBS="-L$ac_silc_libs"
	fi
	SILC_LIBS="$SILC_LIBS -lsilc -lsilcclient -lpthread $LIBDL"
	AC_CHECK_LIB(silcclient, silc_client_init, [silcclient=yes], , $SILC_LIBS)

	if test "x$silcincludes" = "xyes" -a "x$silcclient" = "xyes"; then
		have_silc="yes"
	else
		CPPFLAGS_save="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $SILC_CFLAGS"
		AC_CHECK_HEADER(silcincludes.h, [silc10includes=yes])
		CPPFLAGS="$CPPFLAGS_save"

		SILC_LIBS="$SILC_LIBS -lsilc -lsilcclient -lpthread $LIBDL"
		AC_CHECK_LIB(silcclient, silc_client_init, [silc10client=yes], , $SILC_LIBS)
		if test "x$silc10includes" = "xyes" -a "x$silc10client" = "xyes"; then
			have_silc="yes"
		fi
	fi
fi
AC_SUBST(SILC_LIBS)
AC_SUBST(SILC_CFLAGS)
dnl SILC Toolkit >= 1.0.1 has a new MIME API
if test "x$silcclient" = "xyes"; then
	AC_DEFINE(HAVE_SILCMIME_H, 1, [Define if we have silcmime.h])
elif test "x$silc10client" = "xyes"; then
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SILC_CFLAGS"
		AC_MSG_CHECKING(for silcmime.h)
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <silcincludes.h>
#include <silcmime.h>
		]], [[]])], [
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_SILCMIME_H, 1, [Define if we have silcmime.h])
		], [
		AC_MSG_RESULT(no)
		])
	CPPFLAGS="$CPPFLAGS_save"
fi

dnl #######################################################################
dnl # Check for Gadu-Gadu client includes and libraries
dnl #######################################################################
AC_ARG_WITH(gadu-includes, [AC_HELP_STRING([--with-gadu-includes=DIR], [compile the Gadu-Gadu plugin against includes in DIR])], [ac_gadu_includes="$withval"], [ac_gadu_includes="no"])
AC_ARG_WITH(gadu-libs, [AC_HELP_STRING([--with-gadu-libs=DIR], [compile the Gadu-Gadu plugin against the libs in DIR])], [ac_gadu_libs="$withval"], [ac_gadu_libs="no"])
GADU_CFLAGS=""
GADU_LIBS=""
if test -n "$with_gadu_includes" || test -n "$with_gadu_libs"; then
	gadu_manual_check="yes"
else
	gadu_manual_check="no"
fi
if test "x$gadu_manual_check" = "xno"; then
	PKG_CHECK_MODULES(GADU, [libgadu >= 1.9.0-rc2], [
		gadu_includes="yes"
		gadu_libs="yes"
	], [
		gadu_includes="no"
	])
else
	if test "$ac_gadu_includes" != "no"; then
		GADU_CFLAGS="-I$ac_gadu_includes"
	fi
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $GADU_CFLAGS"
	AC_CHECK_HEADER(libgadu.h, [gadu_includes=yes])
	CPPFLAGS="$CPPFLAGS_save"

	if test "$ac_gadu_libs" != "no"; then
		GADU_LIBS="-L$ac_gadu_libs"
	fi
	GADU_LIBS="$GADU_LIBS -lgadu"
	AC_CHECK_LIB(gadu, gg_libgadu_version, [gadu_libs=yes], , $GADU_LIBS)
fi
GADU_CFLAGS=`echo $GADU_CFLAGS |$sedpath 's/-Wall//'`

if test "x$gadu_libs" = "xyes"; then
	AC_MSG_CHECKING(for libgadu GPL compatibility)
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $GADU_CFLAGS"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <libgadu.h>]], [[
#if defined(__GG_LIBGADU_HAVE_OPENSSL) || defined(GG_CONFIG_HAVE_OPENSSL)
#error "libgadu is not compatible with the GPL when compiled with OpenSSL support."
#endif
	]])], [
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <libgadu.h>]], [[
#if GG_DEFAULT_PROTOCOL_VERSION < 0x2e
#error "Your libgadu version is too old. libpurple requires 1.9.0-rc2 or higher."
#endif
		]])], [
			AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_LIBGADU], [1],
				[Define to 1 if you have libgadu.])
		], [
			AC_MSG_RESULT(no)
			echo
			echo
			echo "Your supplied copy of libgadu is too old."
			echo "Install version 1.9.0-rc2 or newer."
			echo "Then rerun this ./configure"
			echo
			echo "Falling back to using our own copy of libgadu"
			echo
			GADU_LIBS=""
			GADU_CFLAGS=""
			gadu_libs=no
		])
	], [
		AC_MSG_RESULT(no)
		echo
		echo
		echo "libgadu is not compatible with the GPL when compiled with OpenSSL support."
		echo "To compile against system libgadu, please recompile libgadu using:"
		echo "./autogen.sh --disable-libgadu-openssl --disable-static --enable-shared"
		echo "Then rerun this ./configure"
		echo
		echo "Falling back to using our own copy of libgadu"
		echo
		GADU_LIBS=""
		GADU_CFLAGS=""
		gadu_libs=no
	])
	CPPFLAGS="$CPPFLAGS_save"
fi

AM_CONDITIONAL(USE_INTERNAL_LIBGADU, test "x$gadu_libs" != "xyes")

if test "x$gadu_libs" = "x"; then
	gadu_libs=no
fi

AC_SUBST(GADU_LIBS)
AC_SUBST(GADU_CFLAGS)

AC_ARG_ENABLE(distrib,,,enable_distrib=no)
AM_CONDITIONAL(DISTRIB, test "x$enable_distrib" = "xyes")
DYNAMIC_PRPLS=all
AC_ARG_WITH(static-prpls, [AC_HELP_STRING([--with-static-prpls], [Link to certain protocols statically])], [STATIC_PRPLS=`echo $withval | $sedpath 's/,/ /g'`], [STATIC_PRPLS=""])
if test "x$STATIC_PRPLS" != "x" -a "x$DYNAMIC_PRPLS" = "xall"; then
	DYNAMIC_PRPLS=""
fi

if test "x$STATIC_PRPLS" = "xall" ; then
	STATIC_PRPLS="bonjour gg irc jabber msn myspace mxit novell oscar qq sametime silc simple yahoo zephyr"
fi
if test "x$have_meanwhile" != "xyes" ; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/sametime//'`
fi
if test "x$avahiincludes" != "xyes" -o "x$avahilibs" != "xyes"; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/bonjour//'`
fi
if test "x$silcincludes" != "xyes" -o "x$silcclient" != "xyes"; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/silc/silc10/'`
fi
if test "x$silc10includes" != "xyes" -o "x$silc10client" != "xyes"; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/silc10//'`
fi
AC_SUBST(STATIC_PRPLS)
STATIC_LINK_LIBS=
extern_init=
load_proto=
for i in $STATIC_PRPLS ; do
	dnl Ugly special case for "libsilcpurple.la":
	dnl ... and Ugly special case for multi-protocol oscar and yahoo
	if test \( "x$i" = "xoscar" -o "x$i" = "xaim" -o "x$i" = "xicq" \) -a "x$static_oscar" != "xyes"; then
		STATIC_LINK_LIBS="$STATIC_LINK_LIBS \$(top_builddir)/libpurple/protocols/oscar/liboscar.la"
		extern_init="$extern_init extern gboolean purple_init_aim_plugin();"
		extern_init="$extern_init extern gboolean purple_init_icq_plugin();"
		load_proto="$load_proto purple_init_aim_plugin();"
		load_proto="$load_proto purple_init_icq_plugin();"
	elif test "x$i" = "xyahoo"; then
		STATIC_LINK_LIBS="$STATIC_LINK_LIBS \$(top_builddir)/libpurple/protocols/yahoo/libymsg.la"
		extern_init="$extern_init extern gboolean purple_init_yahoo_plugin();"
		extern_init="$extern_init extern gboolean purple_init_yahoojp_plugin();"
		load_proto="$load_proto purple_init_yahoo_plugin();"
		load_proto="$load_proto purple_init_yahoojp_plugin();"
	else
		if test "x$i" = "xsilc"; then
			STATIC_LINK_LIBS="$STATIC_LINK_LIBS \$(top_builddir)/libpurple/protocols/$i/lib${i}purple.la"
		elif test "x$i" = "xsilc10"; then
			STATIC_LINK_LIBS="$STATIC_LINK_LIBS \$(top_builddir)/libpurple/protocols/$i/libsilcpurple.la"
		else
			STATIC_LINK_LIBS="$STATIC_LINK_LIBS \$(top_builddir)/libpurple/protocols/$i/lib$i.la"
		fi
		extern_init="$extern_init extern gboolean purple_init_${i}_plugin();"
		load_proto="$load_proto purple_init_${i}_plugin();"
	fi
	case $i in
		bonjour)	static_bonjour=yes ;;
		gg)			static_gg=yes ;;
		irc)		static_irc=yes ;;
		jabber)		static_jabber=yes ;;
		msn)		static_msn=yes ;;
		myspace)	static_myspace=yes ;;
		mxit)		static_mxit=yes ;;
		novell)		static_novell=yes ;;
		oscar)		static_oscar=yes ;;
		aim)		static_oscar=yes ;;
		icq)		static_oscar=yes ;;
		qq)			static_qq=yes ;;
		sametime)	static_sametime=yes ;;
		silc)		static_silc=yes ;;
		silc10)		static_silc=yes ;;
		simple)		static_simple=yes ;;
		yahoo)		static_yahoo=yes ;;
		zephyr)		static_zephyr=yes ;;
		*)			echo "Invalid static protocol $i!!" ; exit ;;
	esac
done
AM_CONDITIONAL(STATIC_BONJOUR, test "x$static_bonjour" = "xyes")
AM_CONDITIONAL(STATIC_GG, test "x$static_gg" = "xyes")
AM_CONDITIONAL(STATIC_IRC, test "x$static_irc" = "xyes")
AM_CONDITIONAL(STATIC_JABBER, test "x$static_jabber" = "xyes")
AM_CONDITIONAL(STATIC_MSN, test "x$static_msn" = "xyes")
AM_CONDITIONAL(STATIC_MYSPACE, test "x$static_myspace" = "xyes")
AM_CONDITIONAL(STATIC_MXIT, test "x$static_mxit" = "xyes")
AM_CONDITIONAL(STATIC_NOVELL, test "x$static_novell" = "xyes")
AM_CONDITIONAL(STATIC_OSCAR, test "x$static_oscar" = "xyes")
AM_CONDITIONAL(STATIC_QQ, test "x$static_qq" = "xyes")
AM_CONDITIONAL(STATIC_SAMETIME, test "x$static_sametime" = "xyes" -a "x$have_meanwhile" = "xyes")
AM_CONDITIONAL(STATIC_SILC, test "x$static_silc" = "xyes" -a "x$have_silc" = "xyes")
AM_CONDITIONAL(STATIC_SIMPLE, test "x$static_simple" = "xyes")
AM_CONDITIONAL(STATIC_YAHOO, test "x$static_yahoo" = "xyes")
AM_CONDITIONAL(STATIC_ZEPHYR, test "x$static_zephyr" = "xyes")
AC_SUBST(STATIC_LINK_LIBS)
AC_DEFINE_UNQUOTED(STATIC_PROTO_INIT, $extern_init static void static_proto_init(void) { $load_proto },
	[Loads static protocol plugin module initialization functions.])

AC_ARG_WITH(dynamic_prpls, [AC_HELP_STRING([--with-dynamic-prpls], [specify which protocols to build dynamically])], [DYNAMIC_PRPLS=`echo $withval | $sedpath 's/,/ /g'`])
if test "x$DYNAMIC_PRPLS" = "xall" ; then
	DYNAMIC_PRPLS="bonjour gg irc jabber msn myspace mxit novell oscar qq sametime silc simple yahoo zephyr"
fi
if test "x$have_meanwhile" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/sametime//'`
fi
if test "x$avahiincludes" != "xyes" -o "x$avahilibs" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/bonjour//'`
fi
if test "x$silcincludes" != "xyes" -o "x$silcclient" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/silc/silc10/'`
fi
if test "x$silc10includes" != "xyes" -o "x$silc10client" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/silc10//'`
fi
AC_SUBST(DYNAMIC_PRPLS)
for i in $DYNAMIC_PRPLS ; do
	case $i in
		bonjour)	dynamic_bonjour=yes ;;
		gg)			dynamic_gg=yes ;;
		irc)		dynamic_irc=yes ;;
		jabber)		dynamic_jabber=yes ;;
		msn)		dynamic_msn=yes ;;
		myspace)	dynamic_myspace=yes ;;
		mxit)		dynamic_mxit=yes ;;
		novell)		dynamic_novell=yes ;;
		null)		dynamic_null=yes ;;
		oscar)		dynamic_oscar=yes ;;
		aim)		dynamic_oscar=yes ;;
		icq)		dynamic_oscar=yes ;;
		qq)			dynamic_qq=yes ;;
		sametime)	dynamic_sametime=yes ;;
		silc)		dynamic_silc=yes ;;
		silc10)		dynamic_silc=yes ;;
		simple)		dynamic_simple=yes ;;
		yahoo)		dynamic_yahoo=yes ;;
		zephyr)		dynamic_zephyr=yes ;;
		*)			echo "Invalid dynamic protocol $i!!" ; exit ;;
	esac
done

AC_ARG_ENABLE(plugins, [AC_HELP_STRING([--disable-plugins], [compile without plugin support])], , enable_plugins=yes)
AC_ARG_WITH(krb4, [AC_HELP_STRING([--with-krb4=PREFIX], [compile Zephyr plugin with Kerberos 4 support])], kerberos="$withval", kerberos="no")
AC_ARG_WITH(zephyr, [AC_HELP_STRING([--with-zephyr=PREFIX], [compile Zephyr plugin against external libzephyr])], zephyr="$withval", zephyr="no")
AM_CONDITIONAL(EXTERNAL_LIBZEPHYR, test "x$zephyr" != "xno")

AC_CHECK_HEADERS(sys/utsname.h)
AC_CHECK_FUNC(uname)

AC_ARG_ENABLE(fortify, [AC_HELP_STRING([--disable-fortify], [compile without FORTIFY_SOURCE support])], , enable_fortify=yes)

DEBUG_CFLAGS="$DEBUG_CFLAGS -DPURPLE_DISABLE_DEPRECATED -DPIDGIN_DISABLE_DEPRECATED -DFINCH_DISABLE_DEPRECATED -DGNT_DISABLE_DEPRECATED"
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
	dnl		Perhaps we could write ugly configure magic and pass -Wno-float-equal down to that subdirectory.
	dnl		On the other hand, it's probably actually broken, so maybe the Perl folks should fix that?
	dnl
	dnl Consider removing -Wno-sign-compare (from the -Wextra set) and fixing all those cases.
	dnl	This is likely non-trivial.
	dnl
	for newflag in \
			"-Waggregate-return" \
			"-Wcast-align" \
			"-Wdeclaration-after-statement" \
			"-Wendif-labels" \
			"-Werror-implicit-function-declaration" \
			"-Wextra -Wno-sign-compare -Wno-unused-parameter" \
			"-Wformat-security" \
				"-Werror=format-security" \
			"-Winit-self" \
			"-Wmissing-declarations" \
			"-Wmissing-noreturn" \
			"-Wmissing-prototypes" \
			"-Wpointer-arith" \
			"-Wundef" \
	; do
		orig_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $newflag"
		AC_MSG_CHECKING(for $newflag option to gcc)
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
			int main() {return 0;}
		]])], [
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
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <features.h>]], [[
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
		]])], [
			AC_MSG_RESULT(yes)
			DEBUG_CFLAGS="$DEBUG_CFLAGS -Wp,-D_FORTIFY_SOURCE=2"
		], [
			AC_MSG_RESULT(no)
		])
	fi

	DEBUG_CFLAGS="-Wall $DEBUG_CFLAGS"
	CFLAGS="-g $CFLAGS"
fi

if test "x$SUNCC" = "xyes"; then
        CFLAGS="$CFLAGS -features=extensions" 
fi
AC_SUBST(CFLAGS)

AC_PATH_PROG(pidginpath, pidgin)

dnl #######################################################################
dnl # Check for D-Bus libraries
dnl #######################################################################

AC_ARG_ENABLE(dbus, [AC_HELP_STRING([--disable-dbus], [disable D-Bus support])], , enable_dbus=yes)
AC_ARG_ENABLE(nm, [AC_HELP_STRING([--disable-nm], [disable NetworkManager support (requires D-Bus)])], enable_nm=$enableval, enable_nm=yes)

if test "x$enable_dbus" = "xyes" ; then
	AC_CHECK_PROG(enable_dbus, dbus-binding-tool, yes, no)
fi

if test "x$enable_dbus" = "xyes" ; then
	PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60 dbus-glib-1 >= 0.60], [
		AC_SUBST(DBUS_CFLAGS)
		AC_SUBST(DBUS_LIBS)
		enable_dbus=yes
	], [
	enable_dbus=no
	if test "x$force_deps" = "xyes" ; then
		AC_MSG_ERROR([
D-Bus development headers not found.
Use --disable-dbus if you do not need D-Bus support.
])
	fi])
fi

dnl Check for NetworkManager.h; if we don't have it, oh well
if test "x$enable_dbus" = "xyes" ; then
	if test "x$enable_nm" = "xyes" ; then
		PKG_CHECK_MODULES(NETWORKMANAGER, [NetworkManager >= 0.5.0], [
			AC_SUBST(NETWORKMANAGER_CFLAGS)
			AC_SUBST(NETWORKMANAGER_LIBS)
			AC_DEFINE(HAVE_NETWORKMANAGER, 1, [Define if we have NetworkManager.])
		], [
			enable_nm=no
			if test "x$force_deps" = "xyes" ; then
				AC_MSG_ERROR([
NetworkManager development headers not found.
Use --disable-nm if you do not need NetworkManager support.
])
			fi])
	fi
else
	enable_nm=no
fi

dnl #######################################################################
dnl # Check for Python
dnl #######################################################################

dnl Python scripts are used to auto-generate about 3000 lines of C
dnl and XML code that wraps (part of) the existing API so that
dnl it is now accessible through D-Bus.

dnl Python is only required if --enable-dbus is used, and only for
dnl the build process to generate the code, not for running pidgin.
dnl This autogenerated code is system-independent, so in principle we
dnl can generate all of it before shipping.  But I thought adding
dnl auto-generated stuff to the repository is inelegant.
dnl Alternatively, these python scripts could be rewritten
dnl in C (brrrr ...).

AC_ARG_WITH([python],
			AC_HELP_STRING([--with-python=PATH],
						   [which python interpreter to use for dbus code generation]),
			PYTHON=$withval)

if test "x$enable_dbus" = "xyes" || test "x$enable_consoleui" = "xyes" ; then
	if test -z "$PYTHON" -o "x$PYTHON" = "xyes"; then
		AC_PATH_PROG([PYTHON], [python], [no])
	fi

	if test x"$PYTHON" = x"no" ; then
		AC_MSG_WARN([python interpreter not found in your path])
		enable_dbus=no
	fi

	if $PYTHON -c "import sys; sys.exit(sys.version[[:3]] >= '2.4')" ; then
		AC_MSG_WARN([python version >= 2.4 required])
		enable_dbus=no
	fi
fi

dnl ###########################################################################
dnl # Find the D-Bus services dir.
dnl #
dnl # This is a 3 step process that
dnl #
dnl # 1. checks if --with-dbus-services was set, if so use that.
dnl # 2. checks if --prefix was given, if so use that.
dnl # 3. fallbacks to installing into what should be the correct system
dnl #    directories.
dnl #
dnl # This is still prone to error if one of the legacy directories exist
dnl # although a newer dbus is installed.  But I have tried to order the
dnl # directory searching to keep this situation at a minimum.
dnl ###########################################################################
AC_ARG_WITH(dbus-services, [AC_HELP_STRING([--with-dbus-services=<dir>], [where the D-Bus services directory is located.])])

DBUS_SERVICES_DIR=""

if test x"$enable_dbus" = "xyes" ; then
	AC_MSG_CHECKING([location of the D-Bus services directory])
	if ! test -z "$with_dbus_services" ; then
		if ! test -d "$with_dbus_services" ; then
			AC_MSG_ERROR([$with_dbus_services does not exist, if this is the correct location please make sure that it exists.])
		fi

		DBUS_SERVICES_DIR="$with_dbus_services"
	else
		if test x"$prefix" = x"NONE" ; then
			dnl # no prefix given, so we look for the correct dbus system paths.
			dnl # if a prefix is given, we use it.

			serviceprefixes="$prefix/share $prefix/lib /usr/share /usr/local/share"
			DBUS_SERVICES_DIR=""

			for d in $serviceprefixes ; do
				dir="$d/dbus-1/services"
				if test -d $dir ; then
					DBUS_SERVICES_DIR="$dir"
					break
				fi
			done

			if test -z $DBUS_SERVICES_DIR ; then
				AC_MSG_ERROR([D-Bus services directory was not found!  Please use --with-dbus-services and specify its location.])
			fi
		else
			DBUS_SERVICES_DIR="$datadir/dbus-1/services"
		fi
	fi
	AC_MSG_RESULT([$DBUS_SERVICES_DIR])
	AC_DEFINE(HAVE_DBUS, 1, [Define if we are using D-Bus.])
fi
AC_SUBST(DBUS_SERVICES_DIR)

if test "x$enable_dbus" = "xyes" ; then
	echo "Building with D-Bus support"
else
	echo "Building without D-Bus support"
fi

AM_CONDITIONAL(ENABLE_DBUS, test "x$enable_dbus" = "xyes")

dnl Check for Python headers (currently useful only for libgnt)
dnl (Thanks to XChat)
if test "x$enable_consoleui" = "xyes" -a ! -z "$PYTHON" -a x"$PYTHON" != x"no" ; then
	AC_MSG_CHECKING(for Python compile flags)
	PY_PREFIX=`$PYTHON -c 'import sys ; print sys.prefix'`
	PY_EXEC_PREFIX=`$PYTHON -c 'import sys ; print sys.exec_prefix'`
	changequote(<<, >>)dnl
	PY_VERSION=`$PYTHON -c 'import sys ; print sys.version[0:3]'`
	PY_MAJOR=`$PYTHON -c 'import sys ; print sys.version[0:2]'`
	changequote([, ])dnl
	if test -f $PY_PREFIX/include/python$PY_VERSION/Python.h -a "$PY_MAJOR" = "2."; then
		AC_MSG_RESULT()
		AC_CHECK_LIB(pthread, pthread_create, )
		AC_CHECK_LIB(util, openpty, )
		AC_CHECK_LIB(db, dbopen, )
		PY_LIBS="-L$PY_EXEC_PREFIX/lib/python$PY_VERSION/config -lpython$PY_VERSION"
		PY_CFLAGS="-I$PY_PREFIX/include/python$PY_VERSION"
		AC_DEFINE(USE_PYTHON, [1], [Define if python headers are available.])
		dnl Because the above AC_CHECK_LIB get in the way...
		AC_MSG_CHECKING(for Python compile flags)
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT([Can't find Python.h])
		PY_LIBS=""
		PY_CFLAGS=""
	fi
fi
AC_SUBST(PY_CFLAGS)
AC_SUBST(PY_LIBS)

dnl #######################################################################
dnl # Check for Mono support
dnl #######################################################################
AC_ARG_ENABLE(mono, [AC_HELP_STRING([--enable-mono], [compile with Mono runtime support (experimental)])], , enable_mono=no)
if test x"$enable_mono" = x"yes" ; then
	PKG_CHECK_MODULES(MONO, mono, [
		AC_SUBST(MONO_CFLAGS)
		AC_SUBST(MONO_LIBS)
		enable_mono=yes
	], [
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([
Mono development headers not found.
Use --disable-mono if you do not need Mono support.
])
	])
	if test x"$enable_mono" = x"yes"; then
		oldLIBS="$LIBS"
		LIBS="$LIBS $MONO_LIBS"
		AC_MSG_CHECKING(for libmono)
		AC_CHECK_FUNCS(mono_jit_init, [], enable_mono=no)
		LIBS="$oldLIBS"

		oldCPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $MONO_CFLAGS"
		AC_CHECK_HEADERS(mono/jit/jit.h, [], enable_mono=no)
		AC_CHECK_HEADERS(mono/metadata/object.h, [], enable_mono=no)
		CPPFLAGS="$oldCPPFLAGS"

		AC_DEFINE(ENABLE_MONO, 1, [Define if mono enabled.])
	fi
else
	MONO_CFLAGS=
	MONO_LIBS=
	enable_mono=no
fi

AC_SUBST(MONO_CFLAGS)
AC_SUBST(MONO_LIBS)
AM_CONDITIONAL(USE_MONO, test x"$enable_mono" = x"yes")

dnl #######################################################################
dnl # Check for Perl support
dnl #######################################################################
AC_ARG_ENABLE(perl, [AC_HELP_STRING([--disable-perl], [compile without perl scripting])], , enable_perl=yes)

if test "$enable_plugins" = no ; then
	enable_perl=no
fi
looked_for_perl="no"
if test "$enable_perl" = yes ; then
	looked_for_perl="yes"
	AC_PATH_PROG(perlpath, perl)
	AC_MSG_CHECKING(for Perl compile flags)
	PERL_CFLAGS=`$perlpath -MExtUtils::Embed -e ccopts 2>/dev/null`
	if test "_$PERL_CFLAGS" = _ ; then
		AC_MSG_RESULT([not found, building without perl.])
		enable_perl=no
	else
		PERL_LIBS=`$perlpath -MExtUtils::Embed -e ldopts 2>/dev/null |$sedpath 's/-lgdbm //'`
		PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-ldb //'`
		PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lndbm //'`
		if test "$system" = "Linux"; then
			PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lnsl //'`
			PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lposix //'`
		fi
		PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lc //'`
		AC_MSG_RESULT(ok)

		oldLIBS="$LIBS"
		LIBS="$LIBS $PERL_LIBS"
		AC_MSG_CHECKING(for libperl)
		AC_CHECK_FUNCS(perl_run, [], enable_perl=no)
		LIBS="$oldLIBS"

		oldCPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $PERL_CFLAGS"
		AC_CHECK_HEADERS(EXTERN.h)
		AC_CHECK_HEADERS(perl.h, [], enable_perl=no,
		[#if HAVE_EXTERN_H
		 # include <EXTERN.h>
		 #endif])
		CPPFLAGS="$oldCPPFLAGS"
	fi
fi

if test "$enable_perl" = yes ; then
	AC_PROG_PERL_MODULES(ExtUtils::MakeMaker, , have_makemaker=no)

	if test "x$have_makemaker" = "xno"; then
		enable_perl=no
		PERL_CFLAGS=
		PERL_LIBS=
		AM_CONDITIONAL(USE_PERL, false)
		AC_MSG_WARN(Compiling perl requires ExtUtils::MakeMaker)
	else
		AC_DEFINE(HAVE_PERL, [1], [Compile with support for perl])
		AC_SUBST(PERL_CFLAGS)
		AC_SUBST(PERL_LIBS)
		AM_CONDITIONAL(USE_PERL, true)

		dnl This is almost definitely wrong, but in case there's
		dnl something I'm missing, I'll leave it in.
		AC_CHECK_FUNCS(Perl_eval_pv)

		AC_MSG_CHECKING(for old perl)
		PERL_OLD=`$perlpath -e 'if($]<5.006){printf"yes\n";}else{printf"no\n";}'`

		if test "x$PERL_OLD" = "xyes"; then
			AC_DEFINE(OLD_PERL, 1, [Define if old perl is installed.])
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
		fi

		AC_MSG_CHECKING(for DynaLoader.a)
		DYNALOADER_A=`echo $PERL_LDFLAGS | $perlpath -pe 's/^(.* )*([[^ ]]*DynaLoader\.a).*/\2/'`

		dnl Don't check libperl.a if dynaloader.a wasn't found.
		if test -n "$DYNALOADER_A"; then
			AC_MSG_RESULT(yes)

			dnl Find either libperl.a or libperl.so
			AC_MSG_CHECKING(for libperl.a or libperl.so)
			LIBPERL_A=`echo "$PERL_LDFLAGS -L/usr/lib"|$perlpath -e 'foreach (split(/ /, <STDIN>)) { if (/^-L(.*)/) { my $dir=$1; if (\`ls $dir/libperl.so* 2>/dev/null\`) { print "-lperl"; last; }; if (-e "$dir/libperl.a") { print "$dir/libperl.a"; last } } };'`
			if test -z "$LIBPERL_A"; then
				AC_MSG_RESULT(no)
				DYNALOADER_A=
			else
				AC_MSG_RESULT(yes)

				if test "$LIBPERL_A" = "-lperl"; then
					LIBPERL_A=
				fi
			fi

			PERL_LIBS=`echo $PERL_LIBS | $perlpath -pe 's/^(.* )*[[^ ]]*DynaLoader\.a/\1libperl_dynaloader.la/'`

			if test -n "$LIBPERL_A"; then
				PERL_LIBS=`echo $PERL_LDFLAGS | $sedpath -e 's/-lperl /libperl_orig.la /' -e 's/-lperl$/libperl_orig.la$/'`
			fi

			AC_SUBST(DYNALOADER_A)
			AC_SUBST(LIBPERL_A)
		else
			AC_MSG_RESULT(no)
		fi
	fi
else
	PERL_CFLAGS=
	PERL_LIBS=
	AM_CONDITIONAL(USE_PERL, false)
fi

if test "x$looked_for_perl" = "xyes" -a "x$enable_perl" = "xno" -a "x$force_deps" = "xyes"; then
	AC_MSG_ERROR([
Perl development headers not found.
Use --disable-perl if you do not need Perl scripting support.
])
fi

dnl #######################################################################
dnl # SSL support
dnl #
dnl # Thanks go to Evolution for the checks.
dnl #######################################################################

AC_ARG_WITH(system-ssl-certs, [AC_HELP_STRING([--with-system-ssl-certs=<dir>], [directory containing system-wide SSL CA certificates])], [ssl_certificates_dir=$withval])

SSL_CERTIFICATES_DIR=""
if ! test -z "$ssl_certificates_dir" ; then
	if test "x$ssl_certificates_dir" = "xyes" ; then
		AC_MSG_ERROR([--with-system-ssl-certs requires that a location is specified, eg. --with-system-ssl-certs=/etc/pki/tls/certs])
	fi
	if ! test -d "$ssl_certificates_dir" ; then
		AC_MSG_ERROR([$ssl_certificates_dir does not exist, if this is the correct location please make sure that it exists.])
	fi
	SSL_CERTIFICATES_DIR="$ssl_certificates_dir"
fi
AC_SUBST(SSL_CERTIFICATES_DIR)
AM_CONDITIONAL(INSTALL_SSL_CERTIFICATES, test "x$SSL_CERTIFICATES_DIR" = "x")

dnl These two are inverses of each other <-- stolen from evolution!

AC_ARG_ENABLE(gnutls,
	[  --enable-gnutls=[yes,no]  attempt to use GnuTLS for SSL support [default=yes]],
	[enable_gnutls="$enableval"],
	[enable_gnutls="yes"])

AC_ARG_ENABLE(nss,
	[  --enable-nss=[yes,no,static]    attempt to use Mozilla libnss for SSL support [default=yes]],
	[enable_nss="$enableval"],
	[enable_nss="yes"])

msg_ssl="None. MSN, Yahoo!, Novell Groupwise and Google Talk will not work without GnuTLS or NSS. OpenSSL is NOT usable!"
looked_for_gnutls="no"
dnl #
dnl # Check for GnuTLS if it's specified.
dnl #
if test "x$enable_gnutls" != "xno"; then
	enable_gnutls="no"
	prefix=`eval echo $prefix`
	looked_for_gnutls="yes"

	AC_ARG_WITH(gnutls-includes,
		[  --with-gnutls-includes=PREFIX   location of GnuTLS includes.],
		[ with_gnutls_includes="$withval" ],
		[ with_gnutls_includes="$prefix/include" ])

	have_gnutls_includes="no"

	if test "x$with_gnutls_includes" != "xno"; then
		CPPFLAGS_save="$CPPFLAGS"

		AC_MSG_CHECKING(for GnuTLS includes)
		AC_MSG_RESULT("")

		CPPFLAGS="$CPPFLAGS -I$with_gnutls_includes"
		AC_CHECK_HEADERS(gnutls/gnutls.h, [ gnutls_includes="yes" ])
		CPPFLAGS="$CPPFLAGS_save"

		if test "x$gnutls_includes" != "xno" -a \
		        "x$gnutls_includes" != "x"; then
			have_gnutls_includes="yes"

			if test "x$with_gnutls_includes" != "xNONE/include"; then
				GNUTLS_CFLAGS="-I$with_gnutls_includes"
			fi
		else
			GNUTLS_CFLAGS=""
		fi
	else
		AC_MSG_CHECKING(for GnuTLS includes)
		AC_MSG_RESULT(no)
	fi

	AC_ARG_WITH(gnutls-libs,
		[AC_HELP_STRING([--with-gnutls-libs=PREFIX], [location of GnuTLS libraries.])],
		[ with_gnutls_libs="$withval" ])

	if test "x$with_gnutls_libs"     != "xno" -a \
	        "x$have_gnutls_includes" != "xno"; then

		LIBS_save="$LIBS"

		case $with_gnutls_libs in
			""|-L*) ;;
			*) with_gnutls_libs="-L$with_gnutls_libs" ;;
		esac

		AC_CACHE_CHECK([for GnuTLS libraries], ac_cv_gnutls_libs,
		[
			LIBS="$LIBS $with_gnutls_libs -lgnutls -lgcrypt"
			AC_TRY_LINK_FUNC(gnutls_init, ac_cv_gnutls_libs="yes", ac_cv_gnutls_libs="no")
			LIBS="$LIBS_save"
		])

		if test "x$ac_cv_gnutls_libs" != "xno"; then
			AC_DEFINE(HAVE_GNUTLS, 1, [Define if you have GnuTLS])
			AC_DEFINE(HAVE_SSL)
			msg_gnutls="GnuTLS"
			GNUTLS_LIBS="$with_gnutls_libs -lgnutls -lgcrypt"

			enable_gnutls="yes"
		else
			GNUTLS_CFLAGS=""
			GNUTLS_LIBS=""
		fi
	else
		AC_MSG_CHECKING(for GnuTLS libraries)
		AC_MSG_RESULT(no)
	fi
else
	GNUTLS_CFLAGS=""
	GNUTLS_LIBS=""
fi

AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)

if test "x$enable_gnutls" = "xyes"; then
	AC_MSG_CHECKING(for gnutls_priority_set_direct and gnutls_priority_set)
	LIBS_save="$LIBS"
	LIBS="$LIBS $GNUTLS_LIBS"
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $GNUTLS_CFLAGS"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <gnutls/gnutls.h>]],
                                        [[gnutls_session s; gnutls_priority_set_direct(s, NULL, NULL); gnutls_priority_set(s, NULL);]])],
	               [AC_DEFINE([HAVE_GNUTLS_PRIORITY_FUNCS], 1,
                                  [Define if your gnutls has gnutls_priority_set_direct and friends])
	                AC_MSG_RESULT(yes)],
	               [AC_MSG_RESULT(no)])
	CPPFLAGS="$CPPFLAGS_save"
        LIBS="$LIBS_save"
fi

if test "x$enable_gnutls" = "xyes"; then
	AC_MSG_CHECKING(for GNUTLS_CERT_INSECURE_ALGORITHM)
	LIBS_save="$LIBS"
	LIBS="$LIBS $GNUTLS_LIBS"
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $GNUTLS_CFLAGS"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <gnutls/gnutls.h>]],
                                        [[unsigned int verify = GNUTLS_CERT_INSECURE_ALGORITHM;]])],
	               [AC_DEFINE([HAVE_GNUTLS_CERT_INSECURE_ALGORITHM], 1,
                                  [Define if your gnutls has the GNUTLS_CERT_INSECURE_ALGORITHM flag])
	                AC_MSG_RESULT(yes)],
	               [AC_MSG_RESULT(no)])
	CPPFLAGS="$CPPFLAGS_save"
        LIBS="$LIBS_save"
fi


AM_CONDITIONAL(USE_GNUTLS, test "x$enable_gnutls" = "xyes")


dnl #
dnl # Check for NSS if it's specified, or if GnuTLS checks failed.
dnl #
looked_for_nss="no"
if test "x$enable_nss" != "xno"; then
	looked_for_nss="yes"

	AC_ARG_WITH(nspr-includes,
		[AC_HELP_STRING([--with-nspr-includes=PREFIX], [specify location of Mozilla nspr4 includes.])],
		[with_nspr_includes="$withval"])

	AC_ARG_WITH(nspr-libs,
		[AC_HELP_STRING([--with-nspr-libs=PREFIX], [specify location of Mozilla nspr4 libs.])],
		[with_nspr_libs="$withval"])

	AC_ARG_WITH(nss-includes,
		[AC_HELP_STRING([--with-nss-includes=PREFIX], [specify location of Mozilla nss3 includes.])],
		[with_nss_includes="$withval"])

	AC_ARG_WITH(nss-libs,
		[AC_HELP_STRING([--with-nss-libs=PREFIX], [specify location of Mozilla nss3 libs.])],
		[with_nss_libs="$withval"])


	if test -n "$with_nspr_includes" || test -n "$with_nspr_libs" || \
	   test -n "$with_nss_includes"  || test -n "$with_nss_libs"  ||
	   test "x$enable_nss" = "xstatic"; then

		nss_manual_check="yes"
	else
		nss_manual_check="no"
	fi

	enable_nss="no"

	if test "x$nss_manual_check" = "xno"; then
		if `$PKG_CONFIG --exists mozilla-nss`; then
			PKG_CHECK_MODULES(NSS, mozilla-nss, [
				have_nss="yes"
			], [
				AC_MSG_RESULT(no)
				have_nss="no"
			])
			mozilla_nspr="mozilla-nspr"
			mozilla_nss="mozilla-nss"
		elif `$PKG_CONFIG --exists nss`; then
			PKG_CHECK_MODULES(NSS, nss, [
				have_nss="yes"
			], [
				AC_MSG_RESULT(no)
				have_nss="no"
			])
			mozilla_nspr="nspr"
			mozilla_nss="nss"
		elif `$PKG_CONFIG --exists microb-engine-nss`; then
			PKG_CHECK_MODULES(NSS, microb-engine-nss, [
				have_nss="yes"
			], [
				AC_MSG_RESULT(no)
				have_nss="no"
			])
			mozilla_nspr="mozilla-nspr"
			mozilla_nss="microb-engine-nss"
		fi

		if test "x$have_nss" = "xyes"; then

			AC_DEFINE(HAVE_NSS, 1, [Define if you have Mozilla NSS])
			AC_DEFINE(HAVE_SSL, 1, [Define if you have SSL])

			msg_nss="Mozilla NSS"
			enable_nss="yes"
		else
			nss_manual_check="yes"
		fi
	fi

	if test "x$nss_manual_check" = "xyes"; then
		mozilla_nss=""
		have_nspr_includes="no"

		if test "x$with_nspr_includes" != "xno"; then
			CPPFLAGS_save=$CPPFLAGS

			AC_MSG_CHECKING(for Mozilla nspr4 includes in $with_nspr_includes)
			AC_MSG_RESULT("")

			CPPFLAGS="$CPPFLAGS -I$with_nspr_includes"
			AC_CHECK_HEADERS(nspr.h prio.h, [ moz_nspr_includes="yes" ])
			CPPFLAGS=$CPPFLAGS_save

			if test "x$moz_nspr_includes" != "xno" -a \
			        "x$moz_nspr_includes" != "x"; then

				have_nspr_includes="yes"
				NSPR_CFLAGS="-I$with_nspr_includes"
			fi
		else
			AC_MSG_CHECKING(for Mozilla nspr4 includes)
			AC_MSG_RESULT(no)

			enable_nss="no"
		fi

		have_nspr_libs="no"

		if test "x$with_nspr_libs"     != "xno" -a \
		        "x$have_nspr_includes" != "xno"; then

			CFLAGS_save=$CFLAGS
			LDFLAGS_save=$LDFLAGS

			if test "$enable_nss" = "static"; then
				if test -z "$with_nspr_libs"; then
					AC_MSG_ERROR(
						[Static linkage requested, but path to nspr libraries not set.]
						[Please specify the path to libnspr4.a]
						[Example: --with-nspr-libs=/usr/lib])

					enable_nss="no"
				else
					nsprlibs="$LIBDL $with_nspr_libs/libplc4.a $with_nspr_libs/libplds4.a $with_nspr_libs/libnspr4.a $PTHREAD_LIB"
				fi
			else
				nsprlibs="$LIBDL -lplc4 -lplds4 -lnspr4 $PTHREAD_LIB"
			fi

			AC_CACHE_CHECK([for Mozilla nspr libraries], ac_cv_moz_nspr_libs,
			[
				LIBS_save=$LIBS
				CFLAGS="$CFLAGS $NSPR_CFLAGS"

				LIBS="$nsprlibs"

				if test "x$with_nspr_libs" != "x"; then
					LDFLAGS="$LDFLAGS -L$with_nspr_libs"
				else
					LDFLAGS="$LDFLAGS"
				fi

				AC_TRY_LINK_FUNC(PR_Init,
					[ac_cv_moz_nspr_libs="yes"],
					[ac_cv_moz_nspr_libs="no"])

				CFLAGS=$CFLAGS_save
				LDFLAGS=$LDFLAGS_save
				LIBS=$LIBS_save
			])

			if test "x$ac_cv_moz_nspr_libs" != "xno"; then
				have_nspr_libs="yes"
				NSPR_LIBS="-L$with_nspr_libs $nsprlibs"
			else
				NSPR_CFLAGS=""
				enable_nss="no"
			fi
		else
			AC_MSG_CHECKING(for Mozilla nspr4 libraries)
			AC_MSG_RESULT(no)
		fi

		have_nss_includes="no"

		if test "x$with_nss_includes" != "xno" -a \
				"x$have_nspr_libs"    != "xno"; then

			CPPFLAGS_save=$CPPFLAGS

			AC_MSG_CHECKING(for Mozilla nss3 includes in $with_nss_includes)
			AC_MSG_RESULT("")

			if test "x$with_nspr_includes" != "x"; then
				CPPFLAGS="$CPPFLAGS -I$with_nspr_includes -I$with_nss_includes"
			else
				CPPFLAGS="$CPPFLAGS -I$with_nss_includes"
			fi

			AC_CHECK_HEADERS(nss.h ssl.h smime.h,
				[moz_nss_includes="yes"],
				[moz_nss_includes="no"])

			CPPFLAGS=$CPPFLAGS_save

			if test "x$moz_nss_includes" = "xyes"; then
				have_nss_includes="yes"
				NSS_CFLAGS="-I$with_nss_includes"
			else
				NSPR_CFLAGS=""
				NSPR_LIBS=""
				enable_nss="no"
			fi
		else
			AC_MSG_CHECKING(for Mozilla nss3 includes)
			AC_MSG_RESULT(no)
			enable_nss="no"
		fi

		if test "x$with_nss_libs"     != "xno" -a \
				"x$have_nss_includes" != "xno"; then

			LDFLAGS_save=$LDFLAGS

			if test "$enable_nss" = "static"; then
				if test -z "$with_nss_libs"; then
					AC_MSG_ERROR(
						[Static linkage requested, but path to nss libraries not set.]
						[Please specify the path to libnss3.a]
						[Example: --with-nspr-libs=/usr/lib/mozilla])
					enable_nss="no"
				else
					nsslibs="-ldb1 $with_nss_libs/libnssckfw.a $with_nss_libs/libasn1.a $with_nss_libs/libcrmf.a $with_nss_libs/libswfci.a $with_nss_libs/libjar.a $with_nss_libs/libpkcs12.a $with_nss_libs/libpkcs7.a $with_nss_libs/libpki1.a $with_nss_libs/libsmime.a $with_nss_libs/libssl.a $with_nss_libs/libnss.a $with_nss_libs/libpk11wrap.a $with_nss_libs/libsoftokn.a $with_nss_libs/libfreebl.a $with_nss_libs/libnsspki.a $with_nss_libs/libnssdev.a $with_nss_libs/libcryptohi.a $with_nss_libs/libcerthi.a $with_nss_libs/libcertdb.a $with_nss_libs/libsecutil.a $with_nss_libs/libnssb.a"

					case "$host" in
						*solaris*)
							nsslibs="$nsslibs $with_nss_libs/libfreeb1.a"
							;;
					esac
				fi
			else
				nsslibs="-lssl3 -lsmime3 -lnss3 -lsoftokn3"
			fi

			AC_CACHE_CHECK([for Mozilla nss libraries], ac_cv_moz_nss_libs,
			[
				LIBS_save=$LIBS
				LDFLAGS="$LDFLAGS -L$with_nspr_libs -L$with_nss_libs"
				LIBS="$nsslibs $nsprlibs"

				AC_TRY_LINK_FUNC(NSS_Init,
					[ac_cv_moz_nss_libs="yes"],
					[ac_cv_moz_nss_libs="no"])

				if test "x$ac_cv_moz_nss_libs" = "xno"; then
					nsslibs="-lssl3 -lsmime3 -lnss3 -lsoftokn3"
					LDFLAGS="$LDFLAGS -L$with_nspr_libs -L$with_nss_libs"
					LIBS="$LIBS $nsslibs"
					AC_TRY_LINK_FUNC(NSS_Init,
						[ac_cv_moz_nss_libs="yes"],
						[ac_cv_moz_nss_libs="no"])
				fi

				LDFLAGS=$LDFLAGS_save
				LIBS=$LIBS_save
			])

			if test "x$ac_cv_moz_nss_libs" != "xno"; then
				AC_DEFINE(HAVE_NSS)
				AC_DEFINE(HAVE_SSL)

				NSS_LIBS="-L$with_nss_libs $nsslibs"

				if test "$enable_nss" = "static"; then
					msg_nss="Mozilla NSS (static)"
				else
					msg_nss="Mozilla NSS"
				fi

				enable_nss="yes"
			else
				NSS_CFLAGS=""
				NSPR_CFLAGS=""
				NSPR_LIBS=""
				enable_nss="no"
			fi
		else
			AC_MSG_CHECKING(for Mozilla nss libraries)
			AC_MSG_RESULT(no)
		fi

		NSS_CFLAGS="$NSPR_CFLAGS $NSS_CFLAGS"
		NSS_LIBS="$NSPR_LIBS $NSS_LIBS"
	fi

	AC_SUBST(NSS_CFLAGS)
	AC_SUBST(NSS_LIBS)
fi

AM_CONDITIONAL(USE_NSS, test "x$enable_nss" = "xyes")

if test "x$msg_nss" != "x" -a "x$msg_gnutls" != "x"; then
	msg_ssl="$msg_nss and $msg_gnutls"
elif test "x$msg_nss" != "x"; then
	msg_ssl=$msg_nss
elif test "x$msg_gnutls" != "x"; then
	msg_ssl=$msg_gnutls
elif test "x$looked_for_gnutls" = "xyes" -a "x$looked_for_nss" = "xyes" -a "x$force_deps" = "xyes" ; then
	AC_MSG_ERROR([
Neither GnuTLS or NSS SSL development headers found.
Use --disable-nss --disable-gnutls if you do not need SSL support.
MSN, Yahoo!, Novell Groupwise and Google Talk will not work without GnuTLS or NSS. OpenSSL is NOT usable!
])
elif test "x$looked_for_gnutls" = "xyes" -a "x$force_deps" = "xyes" ; then
	AC_MSG_ERROR([
GnuTLS SSL development headers not found.
Use --disable-gnutls if you do not need SSL support.
MSN, Yahoo!, Novell Groupwise and Google Talk will not work without SSL support.
])
elif test "x$looked_for_nss" = "xyes" -a "x$force_deps" = "xyes" ; then
	AC_MSG_ERROR([
NSS SSL development headers not found.
Use --disable-nss if you do not need SSL support.
MSN, Yahoo!, Novell Groupwise and Google Talk will not work without SSL support.
])
fi

dnl #######################################################################
dnl # Check for Tcl
dnl #######################################################################
AC_ARG_ENABLE(tcl, [AC_HELP_STRING([--disable-tcl],
	[compile without Tcl scripting])], enable_tcl="$enableval", enable_tcl="yes")
AC_ARG_WITH(tclconfig, [AC_HELP_STRING([--with-tclconfig=DIR],
	[directory containing tclConfig.sh])])

if test "$enable_plugins" = no; then
	enable_tcl=no
fi

if test "$enable_tcl" = yes; then
	AC_MSG_CHECKING([for tclConfig.sh])
	TCLCONFIG=no
	TCLCONFIGDIRS="/usr/lib \
			/usr/lib64 \
			/usr/lib/tcl8.5 \
			/usr/lib/tcl8.4 \
			/usr/lib/tcl8.3 \
			/usr/lib/tcl8.2 \
			/usr/lib64/tcl8.5 \
			/usr/lib64/tcl8.4 \
			/System/Library/Tcl/8.3 \
			/usr/local/lib"
	for dir in $with_tclconfig $TCLCONFIGDIRS; do
		if test -f $dir/tclConfig.sh; then
			TCLCONFIG=$dir/tclConfig.sh
			AC_MSG_RESULT([yes ($TCLCONFIG)])
			break
		fi
	done
	if test "$TCLCONFIG" = "no"; then
		AC_MSG_RESULT([no])
		enable_tcl=no
		if test "x$force_deps" = "xyes" ; then
			AC_MSG_ERROR([
Tcl development headers not found.
Use --disable-tcl if you do not need Tcl scripting support.
])
		fi
	else
		. $TCLCONFIG
		AC_MSG_CHECKING([Tcl version compatability])
		if test "$TCL_MAJOR_VERSION" -ne 8 -o "$TCL_MINOR_VERSION" -lt 3; then
			AC_MSG_RESULT([bad, $TCL_VERSION found but 8.3 or later required])
			enable_tcl=no
		else
			AC_MSG_RESULT([ok, $TCL_VERSION])
			eval "TCL_LIB_SPEC=\"$TCL_LIB_SPEC\""
			AC_MSG_CHECKING([for Tcl linkability])
			oldCPPFLAGS=$CPPFLAGS
			CPPFLAGS="$CPPFLAGS $TCL_INCLUDE_SPEC -I$TCL_PREFIX/include"
			oldLIBS=$LIBS
			LIBS="$LIBS $TCL_LIB_SPEC"
			AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <tcl.h>]],
				[[Tcl_Interp *interp=NULL; Tcl_Init(interp)]])],
				[AC_MSG_RESULT([yes]);enable_tcl=yes],
				[AC_MSG_RESULT([no]);enable_tcl=no])
			CPPFLAGS="$oldCPPFLAGS"
			LIBS="$oldLIBS"
		fi
	fi
fi

if test "$enable_tcl" = yes; then
	AM_CONDITIONAL(USE_TCL, true)
	TCL_LIBS=$TCL_LIB_SPEC
	AC_DEFINE(HAVE_TCL, [1], [Compile with support for the Tcl toolkit])
	AC_SUBST(TCL_LIBS)
	TCL_CFLAGS="$TCL_INCLUDE_SPEC -I$TCL_PREFIX/include"
	if test "x$GCC" = "xyes"; then
		TCL_CFLAGS="$TCL_CFLAGS -fno-strict-aliasing"
	fi
	AC_SUBST(TCL_CFLAGS)
else
	AM_CONDITIONAL(USE_TCL, false)
fi

dnl #######################################################################
dnl # Check for Tk
dnl #######################################################################
AC_ARG_ENABLE(tk, [AC_HELP_STRING([--disable-tk],
	[compile without Tcl support for Tk])], enable_tk="$enableval", enable_tk="yes")
AC_ARG_WITH(tkconfig, [AC_HELP_STRING([--with-tkconfig=DIR],
	[directory containing tkConfig.sh])])

if test "$enable_tcl" = yes -a "$enable_tk" = yes; then
	AC_MSG_CHECKING([for tkConfig.sh])
	TKCONFIG=no
	TKCONFIGDIRS="/usr/lib \
			/usr/lib64 \
			/usr/lib/tk8.5 \
			/usr/lib/tk8.4 \
			/usr/lib/tk8.3 \
			/usr/lib/tk8.2 \
			/usr/local/lib"
	for dir in $with_tkconfig $TKCONFIGDIRS; do
		if test -f $dir/tkConfig.sh; then
			TKCONFIG=$dir/tkConfig.sh
			AC_MSG_RESULT([yes ($TKCONFIG)])
			break
		fi
	done
	if test "$TKCONFIG" = "no"; then
		AC_MSG_RESULT([no])
		enable_tk=no
		if test "x$force_deps" = "xyes" ; then
			AC_MSG_ERROR([
Tk development headers not found.
Use --disable-tk if you do not need Tk scripting support.
])
		fi
	else
		. $TKCONFIG
		eval "TK_LIB_SPEC=\"$TK_LIB_SPEC\""
		AC_MSG_CHECKING([for Tk linkability])
		oldCPPFLAGS=$CPPFLAGS
		CPPFLAGS="$CPPFLAGS $TCL_CFLAGS"
		oldLIBS=$LIBS
		LIBS="$LIBS $TCL_LIB_SPEC $TK_LIB_SPEC"
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <tk.h>]],
				[[Tcl_Interp *interp=NULL; Tcl_Init(interp); Tk_Init(interp);]])],
				[AC_MSG_RESULT([yes]);enable_tk=yes],
				[AC_MSG_RESULT([no]);enable_tk=no])
		CPPFLAGS="$oldCPPFLAGS"
		LIBS="$oldLIBS"
	fi
else
	enable_tk=no
fi

if test "$enable_tk" = yes; then
	AM_CONDITIONAL(USE_TK, true)
	AC_DEFINE(HAVE_TK, [1], [Compile with support for the Tk toolkit])
	TK_LIBS=$TK_LIB_SPEC
	AC_SUBST(TK_LIBS)
else
	AM_CONDITIONAL(USE_TK, false)
fi

if test "$ac_cv_cygwin" = yes ; then
	LDADD="$LDADD -static"
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
fi

AC_SUBST(DEBUG_CFLAGS)
AC_SUBST(LDADD)
AC_SUBST(LIBS)

if test "x$enable_plugins" = "xyes" ; then
	AC_DEFINE(PURPLE_PLUGINS, 1, [Define if plugins are enabled.])
	AM_CONDITIONAL(PLUGINS, true)
	PLUGINS_DEFINE="#define PURPLE_PLUGINS 1"
else
	AM_CONDITIONAL(PLUGINS, false)
	PLUGINS_DEFINE="#undef PURPLE_PLUGINS"
fi
AC_SUBST(PLUGINS_DEFINE)

dnl #######################################################################
dnl # Check for Cyrus-SASL (for Jabber)
dnl #######################################################################
dnl AC_CHECK_SIZEOF(short)
AC_CHECK_FUNCS(snprintf connect)
AC_SUBST(SASL_LIBS)
AC_ARG_ENABLE(cyrus-sasl, AC_HELP_STRING([--enable-cyrus-sasl], [enable Cyrus SASL support for jabberd]), enable_cyrus_sasl=$enableval, enable_cyrus_sasl=no)
if test "x$enable_cyrus_sasl" = "xyes" ; then
	AC_CHECK_LIB(sasl2, sasl_client_init, [
			AM_CONDITIONAL(USE_CYRUS_SASL, true)
			AC_DEFINE(HAVE_CYRUS_SASL, [1], [Define to 1 if Cyrus SASL is present])
			SASL_LIBS=-"lsasl2"
		], [
			AM_CONDITIONAL(USE_CYRUS_SASL, false)
			AC_MSG_ERROR([Cyrus SASL library not found])
		])
else
	AM_CONDITIONAL(USE_CYRUS_SASL, false)
fi

dnl #######################################################################
dnl # Check for Kerberos (for Zephyr)
dnl #######################################################################
AC_DEFINE(ZEPHYR_INT32, long, [Size of an int32.])
AC_SUBST(KRB4_CFLAGS)
AC_SUBST(KRB4_LDFLAGS)
AC_SUBST(KRB4_LIBS)
if test "$kerberos" != "no" ; then
	if test "$kerberos" != "yes" ; then
		KRB4_CFLAGS="-I${kerberos}/include"
		if test -d "$kerberos/include/kerberosIV" ; then
			KRB4_CFLAGS="$KRB4_CFLAGS -I${kerberos}/include/kerberosIV"
		fi
		KRB4_LDFLAGS="-L${kerberos}/lib"
	elif test -d /usr/local/include/kerberosIV ; then
		KRB4_CFLAGS="-I/usr/local/include/kerberosIV"
	elif test -d /usr/include/kerberosIV ; then
		KRB4_CFLAGS="-I/usr/include/kerberosIV"
	fi
	AC_DEFINE(ZEPHYR_USES_KERBEROS, 1, [Define if kerberos should be used in Zephyr.])

	orig_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS $KRB4_LDFLAGS"
	AC_CHECK_LIB(krb4, krb_rd_req,
			[KRB4_LIBS="-lkrb4 -ldes425 -lkrb5 -lk5crypto -lcom_err"],
			[AC_CHECK_LIB(krb, krb_rd_req,
				[KRB4_LIBS="-lkrb -ldes"],
				[AC_MSG_ERROR([Kerberos 4 libraries not found])],
				-ldes)],
			-ldes425 -lkrb5 -lk5crypto -lcom_err)
	orig_LIBS="$LIBS"
	LIBS="$LIBS $KRB4_LIBS"
	AC_CHECK_FUNCS(krb_set_key krb_rd_req krb_get_lrealm)
	AC_CHECK_FUNCS(krb_get_err_text krb_log)
	LIBS="$orig_LIBS"
	LDFLAGS="$orig_LDFLAGS"
fi

dnl #######################################################################
dnl # Check for external libzephyr
dnl #######################################################################
AC_SUBST(ZEPHYR_CFLAGS)
AC_SUBST(ZEPHYR_LDFLAGS)
AC_SUBST(ZEPHYR_LIBS)
if test "$zephyr" != "no" ; then
	if test "$zephyr" != "yes" ; then
		ZEPHYR_CFLAGS="-I${zephyr}/include"
		ZEPHYR_LDFLAGS="-L${zephyr}/lib"
	elif test -d /usr/athena/include/zephyr ; then
		ZEPHYR_CFLAGS="-I/usr/athena/include"
	elif test -d /usr/include/zephyr ; then
		ZEPHYR_CFLAGS="-I/usr/include"
	elif test -d /usr/local/include/zephyr ; then
		ZEPHYR_CFLAGS="-I/usr/local/include"
	fi
	AC_DEFINE(LIBZEPHYR_EXT, 1 , [Define if external libzephyr should be used.])
	AM_CONDITIONAL(EXTERNAL_LIBZEPHYR, test "x$zephyr" != "xno")
	orig_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS $ZEPHYR_LDFLAGS"
	AC_CHECK_LIB(zephyr, ZInitialize,
		[ZEPHYR_LIBS="-lzephyr"],
		[AC_MSG_ERROR([Zephyr libraries not found])],
		-lzephyr)
	orig_LIBS="$LIBS"
	LIBS="$orig_LIBS"
	LDFLAGS="$orig_LDFLAGS"
fi

AC_MSG_CHECKING(for me pot o' gold)
AC_MSG_RESULT(no)
AC_CHECK_FUNCS(gethostid lrand48)
AC_CHECK_FUNCS(memcpy memmove random strchr strerror vprintf)
AC_CHECK_HEADERS(malloc.h paths.h sgtty.h stdarg.h sys/cdefs.h)
AC_CHECK_HEADERS(sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h)
AC_CHECK_HEADERS(sys/select.h sys/uio.h sys/utsname.h sys/wait.h)
AC_CHECK_HEADERS(termios.h)

# sys/sysctl.h on OpenBSD 4.2 requires sys/param.h
# sys/sysctl.h on FreeBSD requires sys/types.h
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(sys/sysctl.h, [], [],
	[[
		#include <sys/types.h>
		#ifdef HAVE_PARAM_H
		# include <sys/param.h>
		#endif
	]])

AC_CHECK_HEADERS(sys/socket.h)
AC_VAR_TIMEZONE_EXTERNALS

AC_CACHE_CHECK(for tm_gmtoff in struct tm, ac_cv_struct_tm_gmtoff,
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
                #include <time.h>
        ]], [[
                struct tm tm;
                tm.tm_gmtoff = 1;
        ]])], [ac_cv_struct_tm_gmtoff=yes], [ac_cv_struct_tm_gmtoff=no]))
if test $ac_cv_struct_tm_gmtoff = yes; then
        AC_DEFINE(HAVE_TM_GMTOFF, 1, [Define if you have a tm_gmtoff member in struct tm])
fi

AC_CACHE_CHECK([whether va_lists can be copied by value], ac_cv_va_val_copy,[
	AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdarg.h>
#include <stdlib.h>
	void f (int i, ...) {
	va_list args1, args2;
	va_start (args1, i);
	args2 = args1;
	if (va_arg (args2, int) != 42 || va_arg (args1, int) != 42)
	  exit (1);
	va_end (args1); va_end (args2);
	}
	int main() {
	  f (0, 42);
	  return 0;
	}]])],
	[ac_cv_va_val_copy=yes],
	[ac_cv_va_val_copy=no],
	[ac_cv_va_val_copy=yes])
])

if test "x$ac_cv_va_val_copy" = "xno"; then
	AC_DEFINE(VA_COPY_AS_ARRAY, 1, ['va_lists' cannot be copied as values])
fi

dnl #######################################################################
dnl # Check for check
dnl #######################################################################
PKG_CHECK_MODULES(CHECK, [check >= 0.9.4], , [AC_MSG_RESULT([no, testing is disabled])])
AM_CONDITIONAL(HAVE_CHECK, [test "x$CHECK_LIBS" != "x"])
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)

dnl #######################################################################
dnl # Disable pixmap installation
dnl #######################################################################
AC_ARG_ENABLE(pixmaps-install, AC_HELP_STRING([--disable-pixmaps-install], [disable installation of pixmap files - Pidgin still needs them!]), enable_pixmaps="$enableval", enable_pixmaps=yes)

AM_CONDITIONAL(INSTALL_PIXMAPS, test "x$enable_pixmaps" = "xyes")

dnl #######################################################################
dnl # Tweak status tray icon installation directory
dnl #######################################################################
AC_ARG_ENABLE(trayicon-compat, AC_HELP_STRING([--enable-trayicon-compat], [install tray icons in location compatible with older releases of hicolor-icon-theme]), enable_traycompat="$enableval", enable_traycompat=no)

AM_CONDITIONAL(ENABLE_TRAYCOMPAT, test "x$enable_traycompat" = "xyes")

dnl #######################################################################
dnl # Check for Doxygen and dot (part of GraphViz)
dnl #######################################################################
AC_ARG_ENABLE(doxygen,
	[AC_HELP_STRING([--disable-doxygen],
		[disable documentation with doxygen])],
	enable_doxygen="$enableval", enable_doxygen="yes")
AC_ARG_ENABLE(dot,
	[AC_HELP_STRING([--disable-dot],
		[disable graphs in doxygen via 'dot'])],
	enable_dot="$enableval", enable_dot="yes")
AC_ARG_ENABLE(devhelp,
	[AC_HELP_STRING([--disable-devhelp],
		[disable building index for devhelp documentation browser])],
	enable_devhelp="$enableval", enable_devhelp="yes")

if test "x$enable_doxygen" = xyes; then
	AC_CHECK_PROG(DOXYGEN, doxygen, true, false)
	if test $DOXYGEN = false; then
		AC_MSG_WARN([*** Doxygen not found, docs will not be available])
		enable_doxygen="no"
	else
		AC_DEFINE_UNQUOTED(HAVE_DOXYGEN, 1, [whether or not we have doxygen])

		if test "x$enable_dot" = "xyes"; then
			AC_CHECK_PROG(DOT, dot, true, false)

			if test $DOT = false; then
				enable_dot="no";
				AC_MSG_WARN([*** GraphViz dot not found, docs will not have graphs])
			else
				AC_DEFINE_UNQUOTED(HAVE_DOT, 1, [whether or not we have dot])
			fi
		fi

		if test "x$enable_devhelp" = "xyes"; then
			AC_CHECK_PROG(XSLTPROC, xsltproc, true, false)

			if test $XSLTPROC = false; then
				enable_devhelp="no";
				AC_MSG_WARN([*** xsltproc not found; devhelp index will not be created])
			else
				AC_DEFINE_UNQUOTED(HAVE_XSLTPROC, 1, [whether or not we have xsltproc for devhelp index])
			fi
		fi
	fi
else
	enable_dot="no"
	enable_devhelp="no"
fi

AC_SUBST(enable_doxygen)
AC_SUBST(enable_dot)
AC_SUBST(enable_devhelp)
AM_CONDITIONAL(HAVE_DOXYGEN, test "x$enable_doxygen" = "xyes")
AM_CONDITIONAL(HAVE_XSLTPROC, test "x$enable_devhelp" = "xyes")

AC_ARG_ENABLE(debug, [AC_HELP_STRING([--enable-debug],
	[compile with debugging support])], , enable_debug=no)

if test "x$enable_debug" = "xyes" ; then
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
fi

AM_CONDITIONAL(PURPLE_AVAILABLE, true)

AC_CONFIG_FILES([Makefile
		   Doxyfile
		   doc/Makefile
		   doc/pidgin.1
		   doc/finch.1
		   m4macros/Makefile
		   pidgin.apspec
		   pidgin/Makefile
		   pidgin/pidgin.pc
		   pidgin/pidgin-uninstalled.pc
		   pidgin/pidgin-2.pc
		   pidgin/pidgin-2-uninstalled.pc
		   pidgin/pixmaps/Makefile
		   pidgin/pixmaps/emotes/default/24/Makefile
		   pidgin/pixmaps/emotes/none/Makefile
		   pidgin/pixmaps/emotes/small/16/Makefile
		   pidgin/plugins/Makefile
		   pidgin/plugins/cap/Makefile
		   pidgin/plugins/disco/Makefile
		   pidgin/plugins/gestures/Makefile
		   pidgin/plugins/gevolution/Makefile
		   pidgin/plugins/musicmessaging/Makefile
		   pidgin/plugins/perl/Makefile
		   pidgin/plugins/perl/common/Makefile.PL
		   pidgin/plugins/ticker/Makefile
		   libpurple/example/Makefile
		   libpurple/gconf/Makefile
		   libpurple/purple.pc
		   libpurple/purple-uninstalled.pc
		   libpurple/purple-2.pc
		   libpurple/purple-2-uninstalled.pc
		   libpurple/plugins/Makefile
		   libpurple/plugins/mono/Makefile
		   libpurple/plugins/mono/api/Makefile
		   libpurple/plugins/mono/loader/Makefile
		   libpurple/plugins/perl/Makefile
		   libpurple/plugins/perl/common/Makefile.PL
		   libpurple/plugins/ssl/Makefile
		   libpurple/plugins/tcl/Makefile
		   libpurple/Makefile
		   libpurple/protocols/Makefile
		   libpurple/protocols/bonjour/Makefile
		   libpurple/protocols/gg/Makefile
		   libpurple/protocols/irc/Makefile
		   libpurple/protocols/jabber/Makefile
		   libpurple/protocols/msn/Makefile
		   libpurple/protocols/myspace/Makefile
		   libpurple/protocols/mxit/Makefile
		   libpurple/protocols/novell/Makefile
		   libpurple/protocols/null/Makefile
		   libpurple/protocols/oscar/Makefile
		   libpurple/protocols/qq/Makefile
		   libpurple/protocols/sametime/Makefile
		   libpurple/protocols/silc/Makefile
		   libpurple/protocols/silc10/Makefile
		   libpurple/protocols/simple/Makefile
		   libpurple/protocols/yahoo/Makefile
		   libpurple/protocols/zephyr/Makefile
		   libpurple/tests/Makefile
		   libpurple/purple.h
		   libpurple/version.h
		   share/sounds/Makefile
		   share/ca-certs/Makefile
		   finch/finch.pc
		   finch/Makefile
		   finch/libgnt/Makefile
		   finch/libgnt/gnt.pc
		   finch/libgnt/wms/Makefile
		   finch/plugins/Makefile
		   po/Makefile.in
		   pidgin.spec
		  ])
AC_OUTPUT

echo
echo $PACKAGE $VERSION

echo
echo Build GTK+ 2.x UI............. : $enable_gtkui
echo Build console UI.............. : $enable_consoleui
echo Build for X11................. : $with_x
echo
echo Enable Gestures............... : $enable_gestures
echo Protocols to build dynamically : $DYNAMIC_PRPLS
echo Protocols to link statically.. : $STATIC_PRPLS
echo
echo Build with GStreamer support.. : $enable_gst
echo Build with D-Bus support...... : $enable_dbus
echo Build with voice and video.... : $enable_vv
if test "x$enable_dbus" = "xyes" ; then
	eval eval echo D-Bus services directory...... : $DBUS_SERVICES_DIR
fi
echo Build with GNU Libidn......... : $enable_idn
echo Build with NetworkManager..... : $enable_nm
echo SSL Library/Libraries......... : $msg_ssl
if test "x$SSL_CERTIFICATES_DIR" != "x" ; then
	eval eval echo SSL CA certificates directory. : $SSL_CERTIFICATES_DIR
fi
echo Build with Cyrus SASL support. : $enable_cyrus_sasl
echo Use kerberos 4 with zephyr.... : $kerberos
echo Use external libzephyr........ : $zephyr
echo Use external libgadu.......... : $gadu_libs
echo Install pixmaps............... : $enable_pixmaps
echo Old tray icon compatibility... : $enable_traycompat
echo Install translations.......... : $enable_i18n
echo Has you....................... : yes
echo
echo Use XScreenSaver Extension.... : $enable_screensaver
echo Use X Session Management...... : $enable_sm
echo Use startup notification...... : $enable_startup_notification
echo Build with GtkSpell support... : $enable_gtkspell
echo
echo Build with plugin support..... : $enable_plugins
echo Build with Mono support....... : $enable_mono
echo Build with Perl support....... : $enable_perl
echo Build with Tcl support........ : $enable_tcl
echo Build with Tk support......... : $enable_tk
echo
echo Print debugging messages...... : $enable_debug
echo
eval eval echo Pidgin will be installed in $bindir.
if test "x$pidginpath" != "x" ; then
	echo Warning: You have an old copy of Pidgin at $pidginpath.
fi
if test "x$enable_pixmaps" = "xno" ; then
	echo
	echo Warning: You have disabled the installation of pixmap data, but Pidgin
	echo still requires installed pixmaps.  Be sure you know what you are doing.
fi
if test "x$enable_i18n" = "xno" ; then
	echo
	echo Warning: You have disabled the building and installation of translation
	echo data.  This will prevent building pidgin.desktop and the GConf schemas.
	echo Be sure you know what you are doing.
fi
echo
echo configure complete, now type \'make\'
echo

