dnl Process this file with autoconf to produce a configure script.
AC_INIT([libgnt], [0.0.0dev], [gaim-devel@lists.sourceforge.net])
AC_CANONICAL_SYSTEM
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

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

dnl Check for inet_aton
AC_CHECK_FUNC(inet_aton, , [AC_CHECK_LIB(resolv, inet_aton, ,
				         [AC_ERROR(inet_aton not found)])])
AC_CHECK_LIB(resolv, __res_query)
AC_CHECK_LIB(nsl, gethostent)
AC_CHECK_FUNC(socket, ,
              [AC_CHECK_LIB(socket, socket, , [AC_ERROR([socket not found])])])
dnl If all goes well, by this point the previous two checks will have
dnl pulled in -lsocket and -lnsl if we need them.
AC_CHECK_FUNC(getaddrinfo, [AC_DEFINE([HAVE_GETADDRINFO], [1],
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

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.0.0 gobject-2.0 gmodule-2.0],
	[
		AC_SUBST(GLIB_CFLAGS)
		AC_SUBST(GLIB_LIBS)
	],
	[
		AC_MSG_ERROR([
*** GLib 2.0 is required to build Gaim; please make sure you have the GLib
*** development headers installed. The latest version of GLib is
*** always available at http://www.gtk.org/.])
	])


AC_MSG_CHECKING(for me pot o' gold)
AC_MSG_RESULT(no)
AC_CHECK_FUNCS(gethostid lrand48)
AC_CHECK_FUNCS(memcpy memmove random strchr strerror vprintf)
AC_CHECK_HEADERS(malloc.h paths.h sgtty.h stdarg.h sys/cdefs.h)
AC_CHECK_HEADERS(sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h)
AC_CHECK_HEADERS(sys/select.h sys/uio.h sys/utsname.h sys/wait.h)
AC_CHECK_HEADERS(termios.h)
#AC_VAR_TIMEZONE_EXTERNALS

AC_CHECK_LIB(ncursesw, initscr, , [AC_MSG_ERROR([
*** You need ncursesw. ])])

AC_OUTPUT([Makefile
           gnt.pc
		  ])

