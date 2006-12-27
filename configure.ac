dnl Process this file with autoconf to produce a configure script.
AC_INIT([gaim], [2.0.0dev], [gaim-devel@lists.sourceforge.net])
AC_CANONICAL_SYSTEM
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
#AM_INIT_AUTOMAKE([foreign dist-bzip2])

AC_PREREQ([2.50])

AC_PATH_PROG(sedpath, sed)

dnl Storing configure arguments
AC_DEFINE_UNQUOTED(CONFIG_ARGS, "$ac_configure_args", [configure arguments])

dnl Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AM_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL
AC_PROG_INTLTOOL
PKG_PROG_PKG_CONFIG

GETTEXT_PACKAGE=gaim
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

ALL_LINGUAS="am ar az bg bn bs ca ca@valencia cs da de el en_AU en_CA en_GB eo es et eu fa fi fr gl gu he hi hu it ja ka ko ku lt mk my_MM nb ne nl nn pa pl pt_BR pt ro ru sk sl sq sr sr@Latn sv ta te th tr uk vi xh zh_CN zh_TW"
AM_GLIB_GNU_GETTEXT

dnl we don't use autobreak on cygwin!!
dnl AC_CYGWIN

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
				         [AC_ERROR(inet_aton not found)])])
AC_CHECK_LIB(resolv, __res_query)
AC_CHECK_LIB(nsl, gethostent)
AC_CHECK_FUNC(socket, ,
	[AC_CHECK_LIB(socket, socket, , [AC_ERROR([socket not found])])])
dnl If all goes well, by this point the previous two checks will have
dnl pulled in -lsocket and -lnsl if we need them.
AC_CHECK_FUNC(getaddrinfo,
	[AC_DEFINE([HAVE_GETADDRINFO], [1],
		[Define to 1 if you have the getaddrinfo function.])],
	[AC_CHECK_LIB(socket, getaddrinfo,
		[AC_DEFINE([HAVE_GETADDRINFO]) LIBS="-lsocket -lsnl $LIBS"], , , -lnsl)])

dnl Check for socklen_t (in Unix98)
AC_MSG_CHECKING(for socklen_t)
AC_TRY_COMPILE([
	#include <sys/types.h>
	#include <sys/socket.h>
	socklen_t x;
], [],
[
	AC_MSG_RESULT(yes)
], [
	AC_TRY_COMPILE([
		#include <sys/types.h>
		#include <sys/socket.h>
		int accept(int, struct sockaddr *, size_t *);
	], [], [
		AC_MSG_RESULT(size_t)
		AC_DEFINE(socklen_t, size_t, [socklen_t size])
	], [
		AC_MSG_RESULT(int)
		AC_DEFINE(socklen_t, int, [socklen_t size])
	])
])

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
], [
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_STRFTIME_Z_FORMAT], [1],
		[Define to 1 if you have a strftime() that supports the %z format string.])
], [
	AC_MSG_RESULT(no)
], [
	# Fallback for Cross Compiling...
	# This will enable the compatibility code.
	AC_MSG_RESULT(no)
]
)

dnl #######################################################################
dnl # Check for GLib 2.0 (required)
dnl #######################################################################
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.0.0 gobject-2.0 gmodule-2.0 gthread-2.0], , [
	AC_MSG_RESULT(no)
	AC_MSG_ERROR([

You must have the GLib 2.0 development headers installed to build Gaim.
])])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

AC_ARG_ENABLE(gtkui, [AC_HELP_STRING([--disable-gtkui],
		[compile without GTK+ user interface])],
	enable_gtkui="$enableval", enable_gtkui="yes")
AC_ARG_ENABLE(consoleui, [AC_HELP_STRING([--disable-consoleui],
		[compile without console user interface])],
	enable_consoleui=$enableval, enable_consoleui=yes)

dnl #######################################################################
dnl # Check for GTK+ 2.0 and other things used by the GTK UI
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
	[AC_HELP_STRING([--disable-gevolution],
		[compile without the Gaim Evolution plugin])],
	enable_gevolution="$enableval", enable_gevolution="yes")
AC_ARG_ENABLE(cap,
	[AC_HELP_STRING([--disable-cap],
		[compile without Contact Availability Prediction plugin])],
	enable_cap="$enableval", enable_cap="yes")


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
	PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.0.0], , [
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([

You must have the GTK+ 2.0 development headers installed to compile Gaim's
GTK+ interface.  If you only want to build the console interface then
specify --disable-gtkui when running configure.
])])

	AC_SUBST(GTK_CFLAGS)
	AC_SUBST(GTK_LIBS)

	dnl #######################################################################
	dnl # Check for XScreenSaver
	dnl #######################################################################
	if test "x$enable_screensaver" = "xyes" ; then
		old_LIBS="$LIBS"
		LIBS="$LIBS $GTK_LIBS $x_libpath_add"
		XSS_LIBS=""
		XSS_HEADERS=""
		AC_CHECK_LIB(Xext, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_EXTRA_LIBS"],[],[-lX11 -lXext -lm])
		AC_CHECK_LIB(Xss, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_LIBS $X_EXTRA_LIBS -lXss"],[],[-lX11 -lXext -lm])
		if test "x$XSS_LIBS" != "x"; then
			oldCPPFLAGS="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $x_incpath_add"
			AC_TRY_COMPILE([
					#include <X11/Xlib.h>
					#include <X11/extensions/scrnsaver.h>
				], [], [], [enable_screensaver=no])
			CPPFLAGS="$oldCPPFLAGS"
		else
			enable_screensaver=no
		fi
		LIBS="$old_LIBS"

		if test "x$enable_screensaver" = "xyes" ; then
			AC_DEFINE(USE_SCREENSAVER, 1, [Define if we're using XScreenSaver.])
			AC_SUBST(XSS_LIBS)
		fi
	fi

	dnl #######################################################################
	dnl # Check for X session management libs
	dnl #######################################################################
	if test "x$enable_sm" = "xyes"; then
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
		fi
	fi

	dnl #######################################################################
	dnl # Check for startup notification
	dnl #######################################################################
	if test "x$enable_startup_notification" = "xyes"; then
		PKG_CHECK_MODULES(STARTUP_NOTIFICATION, [libstartup-notification-1.0 >= 0.5], , [
			AC_MSG_RESULT(no)
			enable_startup_notification=no
		])

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
			enable_gtkspell=no
		])
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
		fi
	fi

	dnl #######################################################################
	dnl # Check for libsqlite3 (for the Contact Availability Prediction plugin)
	dnl #######################################################################
	if test "x$enable_cap" = "xyes"; then
		PKG_CHECK_MODULES(SQLITE3, sqlite3 >= 3.3,,[
													AC_MSG_RESULT(no)
													enable_cap="no"
													])
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

dnl #######################################################################
dnl # Check for ncurses and other things used by the console UI
dnl #######################################################################
GNT_LIBS=""
GNT_CFLAGS=""
AC_ARG_WITH(ncurses-headers, [AC_HELP_STRING([--with-ncurses-headers=DIR],
		[compile gaim-text against the ncurses includes in DIR])],
		[ac_ncurses_includes="$withval"], [ac_ncurses_includes=""])
if test "x$enable_consoleui" = "xyes"; then
	AC_CHECK_LIB(ncursesw, initscr, [GNT_LIBS="-lncursesw"], [enable_consoleui=no])
	AC_CHECK_LIB(panelw, update_panels, [GNT_LIBS="$GNT_LIBS -lpanelw"], [enable_consoleui=no])

	if test "x$enable_consoleui" = "xyes"; then
		dnl # Some distros put the headers in ncursesw/, some don't
		found_ncurses_h=no
		for location in $ac_ncurses_includes $NCURSES_HEADERS /usr/include/ncursesw /usr/include
		do
			f="$location/ncurses.h"
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
					AC_MSG_RESULT([yes])
					break
				], [
					AC_MSG_RESULT([no])
				])
			])
		done

		if test x"$found_ncurses_h" = x"no" ; then
			GNT_LIBS=""
			GNT_CFLAGS=""
			enable_consoleui=no
			AC_MSG_RESULT([no])
		else
			AC_MSG_RESULT([yes])
		fi
	else
		# ncursesw was not found. Look for plain old ncurses
		enable_consoleui=yes
		AC_CHECK_LIB(ncurses, initscr, [GNT_LIBS="-lncurses"], [enable_consoleui=no])
		AC_CHECK_LIB(panel, update_panels, [GNT_LIBS="$GNT_LIBS -lpanel"], [enable_consoleui=no])
		AC_DEFINE(NO_WIDECHAR, 1, [Define to 1 if you don't have wide-character support.])
		if test x"$ac_ncurses_includes" != "x"; then
			GNT_CFLAGS="-I$ac_ncurses_includes"
		else
			if test x"$NCURSES_HEADERS" != "x"; then
				GNT_CFLAGS="-I$NCURSES_HEADERS"
			fi
		fi
	fi

	PKG_CHECK_MODULES(X11, x11,
		[AC_DEFINE(HAVE_X11, 1, [Define to 1 if you have X11])], [AC_MSG_RESULT(no)])
	AC_SUBST(X11_LIBS)
	AC_SUBST(X11_CFLAGS)
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

You must have libxml2 >= 2.6.0 development headers installed to build Gaim.
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
AC_ARG_ENABLE(gstreamer,
	[AC_HELP_STRING([--disable-gstreamer], [compile without GStreamer audio support])],
	enable_gst="$enableval", enable_gst="yes")
PKG_CHECK_MODULES(GSTREAMER, [gstreamer-0.10], , [
	AC_MSG_RESULT(no)
	enable_gst="no"
])
if test "x$enable_gst" != "xno"; then
	AC_DEFINE(USE_GSTREAMER, 1, [Use GStreamer for playing sounds])
	AC_SUBST(GSTREAMER_CFLAGS)
	AC_SUBST(GSTREAMER_LIBS)
fi

dnl #######################################################################
dnl # Check for Meanwhile headers (for Sametime)
dnl #######################################################################
PKG_CHECK_MODULES(MEANWHILE, [meanwhile >= 1.0.0 meanwhile < 2.0.0], [
	have_meanwhile="yes"
], [
	AC_MSG_RESULT(no)
	have_meanwhile="no"
])
AC_SUBST(MEANWHILE_CFLAGS)
AC_SUBST(MEANWHILE_LIBS)

dnl #######################################################################
dnl # Check for Howl headers (for Bonjour)
dnl #######################################################################
AC_ARG_WITH(howl-includes, [AC_HELP_STRING([--with-howl-includes=DIR], [compile the Bonjour plugin against the Howl includes in DIR])], [ac_howl_includes="$withval"], [ac_howl_includes="no"])
AC_ARG_WITH(howl-libs, [AC_HELP_STRING([--with-howl-libs=DIR], [compile the Bonjour plugin against the Howl libs in DIR])], [ac_howl_libs="$withval"], [ac_howl_libs="no"])
HOWL_CFLAGS=""
HOWL_LIBS=""

dnl Attempt to autodetect avahi-compat-howl
PKG_CHECK_MODULES(HOWL, avahi-compat-howl, [
	howlincludes="yes"
	howllibs="yes"
], [
	AC_MSG_RESULT(no)
	howlincludes="no"
	howllibs="no"
])

dnl Attempt to autodetect Howl
if test "x$howlincludes" = "xno"; then
	PKG_CHECK_MODULES(HOWL, howl, [
		howlincludes="yes"
		howllibs="yes"
	], [
		AC_MSG_RESULT(no)
		howlincludes="no"
		howllibs="no"
	])
fi

dnl Override HOWL_CFLAGS if the user specified an include dir
if test "$ac_howl_includes" != "no"; then
	HOWL_CFLAGS="-I$ac_howl_includes"
fi
CPPFLAGS_save="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $HOWL_CFLAGS"
AC_CHECK_HEADER(howl.h, [howlincludes=yes], [howlincludes=no])
CPPFLAGS="$CPPFLAGS_save"

dnl Override HOWL_LIBS if the user specified a libs dir
if test "$ac_howl_libs" != "no"; then
	HOWL_LIBS="-L$ac_howl_libs -lhowl"
fi
AC_CHECK_LIB(howl, sw_discovery_init, [howllibs=yes], [howllibs=no], $HOWL_LIBS)

AC_SUBST(HOWL_CFLAGS)
AC_SUBST(HOWL_LIBS)

dnl #######################################################################
dnl # Check for SILC client includes and libraries
dnl #######################################################################
AC_ARG_WITH(silc-includes, [AC_HELP_STRING([--with-silc-includes=DIR], [compile the SILC plugin against includes in DIR])], [ac_silc_includes="$withval"], [ac_silc_includes="no"])
AC_ARG_WITH(silc-libs, [AC_HELP_STRING([--with-silc-libs=DIR], [compile the SILC plugin against the SILC libs in DIR])], [ac_silc_libs="$withval"], [ac_silc_libs="no"])
SILC_CFLAGS=""
SILC_LIBS=""
if test -n "$with_silc_includes" || test -n "$with_silc_libs"; then
	silc_manual_check="yes"
else
	silc_manual_check="no"
fi
if test "x$silc_manual_check" = "xno"; then
	PKG_CHECK_MODULES(SILC, silcclient, [
		have_silc="yes"
		silcincludes="yes"
		silcclient="yes"
	], [
		AC_MSG_RESULT(no)
		have_silc="no"
	])
	dnl If silcclient.pc wasn't found, check for just silc.pc
	if test "x$have_silc" = "xno"; then
		PKG_CHECK_MODULES(SILC, silc, [
			have_silc="yes"
			silcincludes="yes"
			silcclient="yes"
		], [
			AC_MSG_RESULT(no)
			have_silc="no"
		])
	fi
else
	if test "$ac_silc_includes" != "no"; then
		SILC_CFLAGS="-I$ac_silc_includes"
	fi
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SILC_CFLAGS"
	AC_CHECK_HEADER(silcincludes.h, [silcincludes=yes])
	CPPFLAGS="$CPPFLAGS_save"

	if test "$ac_silc_libs" != "no"; then
		SILC_LIBS="-L$ac_silc_libs"
	fi
	SILC_LIBS="$SILC_LIBS -lsilc -lsilcclient -lpthread $LIBDL"
	AC_CHECK_LIB(silcclient, silc_client_init, [silcclient=yes], , $SILC_LIBS)
fi
AC_SUBST(SILC_LIBS)
AC_SUBST(SILC_CFLAGS)
dnl SILC Toolkit >= 1.0.1 has a new MIME API
if test "x$silcclient" = "xyes"; then
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SILC_CFLAGS"
		AC_MSG_CHECKING(for silcmime.h)
		AC_TRY_COMPILE([
#include <silcincludes.h>
#include <silcmime.h>
		], [], [
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
	PKG_CHECK_MODULES(GADU, libgadu, [
		gadu_includes="yes"
		gadu_libs="yes"
	], [
		AC_MSG_RESULT(no)
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

if test "x$gadu_libs" = "xyes"; then
	AC_MSG_CHECKING(for libgadu GPL compatibility)
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $GADU_CFLAGS"
	AC_TRY_COMPILE([#include <libgadu.h>], [
#ifdef __GG_LIBGADU_HAVE_OPENSSL
#error "libgadu is not compatible with the GPL when compiled with OpenSSL support."
#endif
	], [
		AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_LIBGADU], [1],
			[Define to 1 if you have libgadu.])
	], [
		AC_MSG_RESULT(no)
		echo
		echo
		echo "libgadu is not compatible with the GPL when compiled with OpenSSL support."
		echo "Please recompile libgadu using:"
		echo "./autogen.sh --disable-libgadu-openssl --disable-static --enable-shared"
		echo "Then rerun this ./configure"
		echo
		echo
		GADU_LIBS=""
		GADU_CFLAGS=""
		gadu_libs=no
	])
	CPPFLAGS="$CPPFLAGS_save"
fi

AM_CONDITIONAL(USE_INTERNAL_LIBGADU, test "x$gadu_libs" != "xyes")

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
	STATIC_PRPLS="bonjour gg irc jabber msn novell oscar qq sametime silc simple yahoo zephyr"
fi
if test "x$have_meanwhile" != "xyes" ; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/sametime//'`
fi
if test "x$howlincludes" != "xyes" -o "x$howllibs" != "xyes"; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/bonjour//'`
fi
if test "x$silcincludes" != "xyes" -o "x$silcclient" != "xyes"; then
	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/silc//'`
fi
AC_SUBST(STATIC_PRPLS)
STATIC_LINK_LIBS=
extern_init=
load_proto=
for i in $STATIC_PRPLS ; do
	dnl Ugly special case for "libsilcgaim.a":
	if test "x$i" = "xsilc"; then
		STATIC_LINK_LIBS="$STATIC_LINK_LIBS protocols/$i/lib${i}gaim.a"
	else
		STATIC_LINK_LIBS="$STATIC_LINK_LIBS protocols/$i/lib$i.a"
	fi
	extern_init="$extern_init extern gboolean gaim_init_${i}_plugin();"
	load_proto="$load_proto gaim_init_${i}_plugin();"
	case $i in
		bonjour)	static_bonjour=yes ;;
		gg)			static_gg=yes ;;
		irc)		static_irc=yes ;;
		jabber)		static_jabber=yes ;;
		msn)		static_msn=yes ;;
		novell)		static_novell=yes ;;
		oscar)		static_oscar=yes ;;
		qq)		static_qq=yes ;;
		sametime)	static_sametime=yes ;;
		silc)		static_silc=yes ;;
		simple)		static_simple=yes ;;
		toc)		static_toc=yes ;;
		yahoo)		static_yahoo=yes ;;
		zephyr)		static_zephyr=yes ;;
		*)			echo "Invalid static protocol $i!!" ; exit ;;
	esac
done
AM_CONDITIONAL(STATIC_BONJOUR, test "x$static_bonjour" = "xyes" -a "x$howlincludes" = "xyes" -a "x$howllibs" = "xyes")
AM_CONDITIONAL(STATIC_GG, test "x$static_gg" = "xyes")
AM_CONDITIONAL(STATIC_IRC, test "x$static_irc" = "xyes")
AM_CONDITIONAL(STATIC_JABBER, test "x$static_jabber" = "xyes")
AM_CONDITIONAL(STATIC_MSN, test "x$static_msn" = "xyes")
AM_CONDITIONAL(STATIC_NOVELL, test "x$static_novell" = "xyes")
AM_CONDITIONAL(STATIC_OSCAR, test "x$static_oscar" = "xyes")
AM_CONDITIONAL(STATIC_QQ, test "x$static_qq" = "xyes")
AM_CONDITIONAL(STATIC_SAMETIME, test "x$static_sametime" = "xyes" -a "x$have_meanwhile" = "xyes")
AM_CONDITIONAL(STATIC_SILC, test "x$static_silc" = "xyes" -a "x$silcincludes" = "xyes" -a "x$silcclient" = "xyes")
AM_CONDITIONAL(STATIC_SIMPLE, test "x$static_simple" = "xyes")
AM_CONDITIONAL(STATIC_TOC, test "x$static_toc" = "xyes")
AM_CONDITIONAL(STATIC_YAHOO, test "x$static_yahoo" = "xyes")
AM_CONDITIONAL(STATIC_ZEPHYR, test "x$static_zephyr" = "xyes")
AC_SUBST(STATIC_LINK_LIBS)
AC_DEFINE_UNQUOTED(STATIC_PROTO_INIT, $extern_init static void static_proto_init() { $load_proto },
	[Loads static protocol plugin module initialization functions.])

AC_ARG_WITH(dynamic_prpls, [AC_HELP_STRING([--with-dynamic-prpls], [specify which protocols to build dynamically])], [DYNAMIC_PRPLS=`echo $withval | $sedpath 's/,/ /g'`])
if test "x$DYNAMIC_PRPLS" = "xall" ; then
	DYNAMIC_PRPLS="bonjour gg irc jabber msn novell oscar qq sametime silc simple yahoo zephyr"
fi
if test "x$have_meanwhile" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/sametime//'`
fi
if test "x$howlincludes" != "xyes" -o "x$howllibs" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/bonjour//'`
fi
if test "x$silcincludes" != "xyes" -o "x$silcclient" != "xyes"; then
	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/silc//'`
fi
AC_SUBST(DYNAMIC_PRPLS)
for i in $DYNAMIC_PRPLS ; do
	case $i in
		bonjour)	dynamic_bonjour=yes ;;
		gg)			dynamic_gg=yes ;;
		irc)		dynamic_irc=yes ;;
		jabber)		dynamic_jabber=yes ;;
		msn)		dynamic_msn=yes ;;
		novell)		dynamic_novell=yes ;;
		oscar)		dynamic_oscar=yes ;;
		qq)		dynamic_qq=yes ;;
		sametime)	dynamic_sametime=yes ;;
		silc)		dynamic_silc=yes ;;
		simple)		dynamic_simple=yes ;;
		toc)		dynamic_toc=yes ;;
		yahoo)		dynamic_yahoo=yes ;;
		zephyr)		dynamic_zephyr=yes ;;
		*)			echo "Invalid dynamic protocol $i!!" ; exit ;;
	esac
done
AM_CONDITIONAL(DYNAMIC_BONJOUR, test "x$dynamic_bonjour" = "xyes" -a "x$bonjourincludes" = "xyes" -a "x$bonjourclient" = "xyes")
AM_CONDITIONAL(DYNAMIC_GG, test "x$dynamic_gg" = "xyes")
AM_CONDITIONAL(DYNAMIC_IRC, test "x$dynamic_irc" = "xyes")
AM_CONDITIONAL(DYNAMIC_JABBER, test "x$dynamic_jabber" = "xyes")
AM_CONDITIONAL(DYNAMIC_MSN, test "x$dynamic_msn" = "xyes")
AM_CONDITIONAL(DYNAMIC_NOVELL, test "x$dynamic_novell" = "xyes")
AM_CONDITIONAL(DYNAMIC_OSCAR, test "x$dynamic_oscar" = "xyes")
AM_CONDITIONAL(DYNAMIC_QQ, test "x$dynamic_qq" = "xyes")
AM_CONDITIONAL(DYNAMIC_SAMETIME, test "x$dynamic_sametime" = "xyes" -a "x$have_meanwhile" = "xyes")
AM_CONDITIONAL(DYNAMIC_SILC, test "x$dynamic_silc" = "xyes" -a "x$silcincludes" = "xyes" -a "x$silcclient" = "xyes")
AM_CONDITIONAL(DYNAMIC_SIMPLE, test "x$dynamic_simple" = "xyes")
AM_CONDITIONAL(DYNAMIC_TOC, test "x$dynamic_toc" = "xyes")
AM_CONDITIONAL(DYNAMIC_YAHOO, test "x$dynamic_yahoo" = "xyes")
AM_CONDITIONAL(DYNAMIC_ZEPHYR, test "x$dynamic_zephyr" = "xyes")

AC_ARG_ENABLE(plugins, [AC_HELP_STRING([--disable-plugins], [compile without plugin support])], , enable_plugins=yes)
AC_ARG_WITH(krb4, [AC_HELP_STRING([--with-krb4=PREFIX], [compile Zephyr plugin with Kerberos 4 support])], kerberos="$withval", kerberos="no")
AC_ARG_WITH(zephyr, [AC_HELP_STRING([--with-zephyr=PREFIX], [compile Zephyr plugin against external libzephyr])], zephyr="$withval", zephyr="no")
AM_CONDITIONAL(EXTERNAL_LIBZEPHYR, test "x$zephyr" != "xno")

AC_CHECK_HEADER(sys/utsname.h)
AC_CHECK_FUNC(uname)

AC_ARG_ENABLE(fortify, [AC_HELP_STRING([--disable-fortify], [compile without FORTIFY_SOURCE support])], , enable_fortify=yes)

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

AC_PATH_PROG(gaimpath, gaim)

dnl #######################################################################
dnl # Check for DBUS libraries
dnl #######################################################################

AC_ARG_ENABLE(dbus, [AC_HELP_STRING([--enable-dbus], [enable DBUS support])], , enable_dbus=yes)

if test "x$enable_dbus" = "xyes" ; then
	AC_CHECK_PROG(enable_dbus, dbus-binding-tool, yes, no)
fi

if test "x$enable_dbus" = "xyes" ; then
	PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.35 dbus-glib-1 >= 0.35], [
		AC_SUBST(DBUS_CFLAGS)
		AC_SUBST(DBUS_LIBS)
		enable_dbus=yes
	], [
		AC_MSG_RESULT(no)
		enable_dbus=no
	])

dnl     Check for libnm_glib; if we don't have it, oh well
	LIBNM_CFLAGS=""
	LIBNM_LIBS=""
	PKG_CHECK_MODULES(LIBNM, libnm_glib,
	[
    		AC_DEFINE(HAVE_LIBNM, 1, [Check to see if we have NetworkManager])
	],
	[
                AC_MSG_RESULT(no)
	])
	AC_SUBST(LIBNM_CFLAGS)
	AC_SUBST(LIBNM_LIBS)
fi

dnl #######################################################################
dnl # Check for Python
dnl #######################################################################

dnl Python scripts are used to auto-generate about 3000 lines of C
dnl and XML code that wraps (part of) the existing Gaim API so that
dnl it is now accessible through DBUS.

dnl Python is only required if --enable-dbus is used, and only for
dnl the build process to generate the code, not for running gaim.
dnl This autogenerated code is system-independent, so in principle we
dnl can generate all of it before shipping.  But I thought adding
dnl auto-generated stuff to the repository is inelegant.
dnl Alternatively, these python scripts could be rewritten
dnl in C (brrrr ...).

AC_ARG_WITH([python],
			AC_HELP_STRING([--with-python=PATH],
						   [which python interpreter to use for dbus code generation]),
			PYTHON=$withval)

if test "x$enable_dbus" = "xyes" ; then
	if test -z "$PYTHON" -o "x$PYTHON" = "xyes"; then
		AC_PATH_PROG([PYTHON], [python], [no])
	fi

	if test x"$python" = x"no" ; then
		AC_MSG_WARN([python interpreter not found in your path])
		enable_dbus=no
	fi
fi

if test "x$enable_dbus" = "xyes" ; then
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

			serviceprefixes="$datadir $libdir /usr/share /usr/local/share"
			DBUS_SERVICES_DIR=""

			for d in $serviceprefixes ; do
				dir="$d/dbus-1/services"
				if test -d $dir ; then
					DBUS_SERVICES_DIR="$dir"
					break
				fi
			done

			if test -z $DBUS_SERVICES_DIR ; then
				AC_MSG_ERROR([D-Bus services directory was not found!  Please use --with-dbus-services and specify it's location.])
			fi
		else
			DBUS_SERVICES_DIR="$datadir/dbus-1/services"
		fi
	fi
	AC_MSG_RESULT([$DBUS_SERVICES_DIR])
	AC_DEFINE(HAVE_DBUS, 1, [Define if we are re using DBUS.])
fi
AC_SUBST(DBUS_SERVICES_DIR)

if test "x$enable_dbus" = "xyes" ; then
	echo "Building with DBUS support"
else
	echo "Building without DBUS support"
fi

AM_CONDITIONAL(ENABLE_DBUS, test "x$enable_dbus" = "xyes")

dnl #######################################################################
dnl # Check for Mono support
dnl #######################################################################
AC_ARG_ENABLE(mono, [AC_HELP_STRING([--enable-mono], [compile with Mono runtime support])], , enable_mono=no)
if test x"$enable_mono" = x"yes" ; then
	AC_MSG_CHECKING(for Mono compile flags)
	MONO_CFLAGS=`pkg-config --cflags mono 2> /dev/null`
	if test $? != 0 ; then
		AC_MSG_RESULT([not found, building without mono.])
		MONO_CFLAGS=
		MONO_LIBS=
		enable_mono=no
	else
		MONO_LIBS=`pkg-config --libs mono 2> /dev/null`
		AC_MSG_RESULT(ok)

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

if test "$enable_perl" = yes ; then
	AC_PATH_PROG(perlpath, perl)
	AC_MSG_CHECKING(for Perl compile flags)
	PERL_CFLAGS=`$perlpath -MExtUtils::Embed -e ccopts 2>/dev/null`
	if test "_$PERL_CFLAGS" = _ ; then
		AC_MSG_RESULT([not found, building without perl.])
		enable_perl = no
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

		if test "x$prefix" != "xNONE"; then
			prefix=`eval echo $prefix`
			PERL_MM_PARAMS="INSTALLDIRS=vendor PREFIX=$prefix"
		fi

		AC_ARG_WITH(perl-lib,
		[AC_HELP_STRING([--with-perl-lib=[site|vendor|DIR]],
			[specify where to install the Perl libraries for gaim. Default is site.])],
		[
			if test "x$withval" = xsite; then
				PERL_MM_PARAMS=""
			elif test "x$withval" = xvendor; then
				if test -z "`$perlpath -v | grep '5\.0'`"; then
					PERL_MM_PARAMS="INSTALLDIRS=vendor"
				else
					PERL_MM_PARAMS="INSTALLDIRS=vendor PREFIX=`perl -e 'use Config; print $Config{prefix}'`"
				fi
			else
				PERL_MM_PARAMS="INSTALLDIRS=vendor PREFIX=$withval"
			fi
		])

		AC_SUBST(PERL_MM_PARAMS)

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

dnl #######################################################################
dnl # SSL support
dnl #
dnl # Thanks go to Evolution for the checks.
dnl #######################################################################

dnl These two are inverses of each other <-- stolen from evolution!

AC_ARG_ENABLE(gnutls,
	[  --enable-gnutls=[yes,no]  attempt to use GnuTLS for SSL support (preferred) [default=yes]],
	[enable_gnutls="$enableval"],
	[enable_gnutls="yes"])

AC_ARG_ENABLE(nss,
	[  --enable-nss=[yes,no,static]    attempt to use Mozilla libnss for SSL support [default=yes]],
	[enable_nss="$enableval"],
	[enable_nss="yes"])

msg_ssl="None (MSN and Google Talk will not work without SSL!)"

dnl #
dnl # Check for GnuTLS if it's specified.
dnl #
if test "x$enable_gnutls" != "xno"; then
	enable_gnutls="no"
	prefix=`eval echo $prefix`

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

			GNUTLS_CFLAGS="-I$with_gnutls_includes"
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

		AC_CACHE_CHECK([for GnuTLS libraries], gnutls_libs,
		[
			LIBS="$LIBS $with_gnutls_libs -lgnutls -lgcrypt"
			AC_TRY_LINK_FUNC(gnutls_init, gnutls_libs="yes", gnutls_libs="no")
			LIBS="$LIBS_save"
		])

		if test "x$gnutls_libs" != "xno"; then
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

AM_CONDITIONAL(USE_GNUTLS, test "x$enable_gnutls" = "xyes")


dnl #
dnl # Check for NSS if it's specified, or if GnuTLS checks failed.
dnl #
if test "x$enable_nss" != "xno"; then

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
		else
			if `$PKG_CONFIG --exists nss`; then
				PKG_CHECK_MODULES(NSS, nss, [
					have_nss="yes"
				], [
					AC_MSG_RESULT(no)
				])
				mozilla_nspr="nspr"
				mozilla_nss="nss"
			fi
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

			AC_CACHE_CHECK([for Mozilla nspr libraries], moz_nspr_libs,
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
					[moz_nspr_libs="yes"],
					[moz_nspr_libs="no"])

				CFLAGS=$CFLAGS_save
				LDFLAGS=$LDFLAGS_save
				LIBS=$LIBS_save
			])

			if test "x$moz_nspr_libs" != "xno"; then
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

			AC_CACHE_CHECK([for Mozilla nss libraries], moz_nss_libs,
			[
				LIBS_save=$LIBS
				LDFLAGS="$LDFLAGS -L$with_nspr_libs -L$with_nss_libs"
				LIBS="$nsslibs $nsprlibs"

				AC_TRY_LINK_FUNC(NSS_Init,
					[moz_nss_libs="yes"],
					[moz_nss_libs="no"])

				if test "x$moz_nss_libs" = "xno"; then
					nsslibs="-lssl3 -lsmime3 -lnss3 -lsoftokn3"
					LDFLAGS="$LDFLAGS -L$with_nspr_libs -L$with_nss_libs" 
					LIBS="$LIBS $nsslibs"
					AC_TRY_LINK_FUNC(NSS_Init,
						[moz_nss_libs="yes"],
						[moz_nss_libs="no"])
				fi

				LDFLAGS=$LDFLAGS_save
				LIBS=$LIBS_save
			])

			if test "x$moz_nss_libs" != "xno"; then
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
			/usr/lib/tcl8.4 \
			/usr/lib/tcl8.3 \
			/usr/lib/tcl8.2 \
			/System/Library/Tcl/8.3 \
			/usr/local/lib"
	for dir in $with_tclconfig $TCLCONFIGDIRS; do
		if test -f $dir/tclConfig.sh; then
			TCLCONFIG=$dir/tclConfig.sh
			AC_MSG_RESULT([yes ($TCLCONFIG)])
		fi
	done
	if test "$TCLCONFIG" = "no"; then
		AC_MSG_RESULT([no])
		enable_tcl=no
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
			AC_TRY_LINK([#include <tcl.h>],
				[Tcl_Interp *interp=NULL; Tcl_Init(interp)],
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
			/usr/lib/tk8.4 \
			/usr/lib/tk8.3 \
			/usr/lib/tk8.2 \
			/usr/local/lib"
	for dir in $with_tkconfig $TKCONFIGDIRS; do
		if test -f $dir/tkConfig.sh; then
			TKCONFIG=$dir/tkConfig.sh
			AC_MSG_RESULT([yes ($TKCONFIG)])
		fi
	done
	if test "$TKCONFIG" = "no"; then
		AC_MSG_RESULT([no])
		enable_tk=no
	else
		. $TKCONFIG
		eval "TK_LIB_SPEC=\"$TK_LIB_SPEC\""
		AC_MSG_CHECKING([for Tk linkability])
		oldCPPFLAGS=$CPPFLAGS
		CPPFLAGS="$CPPFLAGS $TCL_CFLAGS"
		oldLIBS=$LIBS
		LIBS="$LIBS $TCL_LIB_SPEC $TK_LIB_SPEC"
		AC_TRY_LINK([#include <tk.h>],
				[Tcl_Interp *interp=NULL; Tcl_Init(interp); Tk_Init(interp);],
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
	AC_DEFINE(GAIM_PLUGINS, 1, [Define if plugins are enabled.])
	AM_CONDITIONAL(PLUGINS, true)
else
	AM_CONDITIONAL(PLUGINS, false)
fi

dnl #######################################################################
dnl # Check for Cyrus-SASL (for Jabber)
dnl #######################################################################
dnl AC_CHECK_SIZEOF(short)
AC_CHECK_FUNCS(snprintf connect)
AC_SUBST(SASL_LIBS)
AC_ARG_ENABLE(cyrus-sasl, AC_HELP_STRING([--enable-cyrus-sasl], [enable Cyrus SASL support for jabberd]), enable_cyrus_sasl=$enableval, enable_cyrus_sasl=no)
if test "x$enable_cyrus_sasl" = "xyes" ; then
	AC_CHECK_LIB(sasl2, sasl_client_init, [
			AC_DEFINE(HAVE_CYRUS_SASL, [1], [Define to 1 if Cyrus SASL is present])
			SASL_LIBS=-"lsasl2"
		], [
			AC_ERROR(Cyrus SASL library not found)
		])
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
				[AC_ERROR(Kerberos 4 libraries not found)],
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
		[AC_ERROR(Zephyr libraries not found)],
		-lzephyr)
	orig_LIBS="$LIBS"
	LIBS="$orig_LIBS"
	LDFLAGS="$orig_LDFLAGS"
fi

AM_BINRELOC

AC_MSG_CHECKING(for me pot o' gold)
AC_MSG_RESULT(no)
AC_CHECK_FUNCS(gethostid lrand48)
AC_CHECK_FUNCS(memcpy memmove random strchr strerror vprintf)
AC_CHECK_HEADERS(malloc.h paths.h sgtty.h stdarg.h sys/cdefs.h)
AC_CHECK_HEADERS(sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h)
AC_CHECK_HEADERS(sys/select.h sys/uio.h sys/utsname.h sys/wait.h)
AC_CHECK_HEADERS(termios.h)
AC_VAR_TIMEZONE_EXTERNALS

AC_CACHE_CHECK(for tm_gmtoff in struct tm, ac_cv_struct_tm_gmtoff,
        AC_TRY_COMPILE([
                #include <time.h>
        ], [
                struct tm tm;
                tm.tm_gmtoff = 1;
        ], ac_cv_struct_tm_gmtoff=yes, ac_cv_struct_tm_gmtoff=no))
if test $ac_cv_struct_tm_gmtoff = yes; then
        AC_DEFINE(HAVE_TM_GMTOFF, 1, [Define if you have a tm_gmtoff member in struct tm])
fi

dnl #######################################################################
dnl # Check for check
dnl #######################################################################
PKG_CHECK_MODULES(CHECK, check >= 0.9.4, have_check=yes, have_check=no)
AM_CONDITIONAL(HAVE_CHECK, test "x$have_check" = "xyes")
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)

dnl #######################################################################
dnl # Check for Doxygen and dot (part of GraphViz)
dnl #######################################################################
AC_ARG_ENABLE(doxygen,
	[AC_HELP_STRING([--disable-doxygen],
		[enable documentation with doxygen])],
	enable_doxygen="$enableval", enable_doxygen="yes")
AC_ARG_ENABLE(dot,
	[AC_HELP_STRING([--enable-dot],
		[enable graphs in doxygen via 'dot'])],
	enable_dot="$enableval", enable_dot="yes")

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
	fi
else
	enable_dot="no"
fi

AC_SUBST(enable_doxygen)
AC_SUBST(enable_dot)
AM_CONDITIONAL(HAVE_DOXYGEN, test "x$enable_doxygen" = "xyes")

AC_ARG_ENABLE(debug, [AC_HELP_STRING([--enable-debug],
	[compile with debugging support])], , enable_debug=no)
if test "x$enable_debug" = "xyes" ; then
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
fi

AC_ARG_ENABLE(fatal-asserts, [AC_HELP_STRING([--enable-fatal-asserts],
	[make assertions fatal (useful for debugging)])], , enable_fatal_asserts=no)
if test "x$enable_fatal_asserts" = "xyes" ; then
	AC_DEFINE(GAIM_FATAL_ASSERTS, 1, [Define to make assertions fatal (useful for debugging).])
fi

AC_OUTPUT([Makefile
		   Doxyfile
		   gaim.apspec
		   gaim.service
		   doc/Makefile
		   doc/gaim.1
		   doc/gaim-text.1
		   m4macros/Makefile
		   gtk/Makefile
		   gtk/pixmaps/Makefile
		   gtk/pixmaps/buddy_icons/Makefile
		   gtk/pixmaps/buddy_icons/qq/Makefile
		   gtk/pixmaps/smileys/Makefile
		   gtk/pixmaps/smileys/default/Makefile
		   gtk/pixmaps/smileys/none/Makefile
		   gtk/pixmaps/status/Makefile
		   gtk/pixmaps/status/default/Makefile
		   gtk/plugins/Makefile
		   gtk/plugins/cap/Makefile
		   gtk/plugins/gestures/Makefile
		   gtk/plugins/gevolution/Makefile
		   gtk/plugins/musicmessaging/Makefile
		   gtk/plugins/perl/Makefile
		   gtk/plugins/perl/common/Makefile.PL
		   gtk/plugins/ticker/Makefile
		   gtk/sounds/Makefile
		   libgaim/gconf/Makefile
		   libgaim/plugins/Makefile
		   libgaim/plugins/mono/Makefile
		   libgaim/plugins/mono/api/Makefile
		   libgaim/plugins/mono/loader/Makefile
		   libgaim/plugins/perl/Makefile
		   libgaim/plugins/perl/common/Makefile.PL
		   libgaim/plugins/ssl/Makefile
		   libgaim/plugins/tcl/Makefile
		   libgaim/Makefile
		   libgaim/protocols/Makefile
		   libgaim/protocols/bonjour/Makefile
		   libgaim/protocols/gg/Makefile
		   libgaim/protocols/irc/Makefile
		   libgaim/protocols/jabber/Makefile
		   libgaim/protocols/msn/Makefile
		   libgaim/protocols/novell/Makefile
		   libgaim/protocols/oscar/Makefile
		   libgaim/protocols/qq/Makefile
		   libgaim/protocols/sametime/Makefile
		   libgaim/protocols/silc/Makefile
		   libgaim/protocols/simple/Makefile
		   libgaim/protocols/toc/Makefile
		   libgaim/protocols/yahoo/Makefile
		   libgaim/protocols/zephyr/Makefile
		   libgaim/tests/Makefile
		   console/Makefile
		   console/libgnt/Makefile
		   console/libgnt/gnt.pc
		   console/libgnt/wms/Makefile
		   console/plugins/Makefile
		   po/Makefile.in
		   gaim.pc
		   gaim.spec
		  ])

echo
echo $PACKAGE $VERSION

echo
echo Build GTK+ 2.x UI............. : $enable_gtkui
echo Build console UI.............. : $enable_consoleui
echo
echo Protocols to build dynamically : $DYNAMIC_PRPLS
echo Protocols to link statically.. : $STATIC_PRPLS
echo
echo Build with GStreamer support.. : $enable_gst
echo Build with DBUS support....... : $enable_dbus
if test "x$enable_dbus" = "xyes" ; then
	eval eval echo DBUS services directory....... : $DBUS_SERVICES_DIR
fi
echo SSL Library/Libraries......... : $msg_ssl
echo Build with Cyrus SASL support. : $enable_cyrus_sasl
echo Use kerberos 4 with zephyr.... : $kerberos
echo Use external libzephyr........ : $zephyr
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
echo Assertions are fatal.......... : $enable_fatal_asserts
echo
eval eval echo Gaim will be installed in $bindir.
if test "x$gaimpath" != "x" ; then
	echo Warning: You have an old copy of gaim at $gaimpath.
fi
echo
echo configure complete, now type \'make\'
echo
