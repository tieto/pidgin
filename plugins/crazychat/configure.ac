#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(src/crazychat.c)
AM_INIT_AUTOMAKE(CrazyChat, 0.0.1)
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Check for debug
AC_ARG_ENABLE(debug,
[AC_HELP_STRING([--enable-debug], [enable debugging])],
[case "${enableval}" in
	yes) debug=true ;;
	no) debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac
test -z "${enableval}" || debug=false],[debug=true])
AM_CONDITIONAL(ENABLE_DEBUG, test x$debug = xtrue)

# Check for disabling Quicktime
AC_ARG_ENABLE(qt,
[AC_HELP_STRING([--disable-qt], [disable Quicktime])],
[case "${enableval}" in
	yes) qt=true ;;
	no) qt=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --disable-qt) ;;
esac
test -z "${enableval}" || qt=true],[qt=false])
AM_CONDITIONAL(DISABLE_QT, test x$qt = xtrue)

# before gettexting, in case iconv matters
case "$host_os" in
darwin*)
        AC_MSG_CHECKING([for fink])
        if test -d /sw; then
                AC_MSG_RESULT([found, adding /sw to search paths])
                CFLAGS="$CFLAGS -I/sw/include"
        else
                AC_MSG_RESULT([not found])
        fi
        ;;
*)
        ;;
esac

# Checks for libraries.
AC_PATH_X

AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR([
*** GLib 2.0 is required to build CrazyChat; please make sure you have the GLib
*** development headers installed. The latest version of GLib is
*** always available at http://www.gtk.org/.]))
AM_PATH_GTK_2_0(2.0.0,,AC_MSG_ERROR([
*** GTK+ 2.0 is required to build CrazyChat; please make sure you have the GTK+
*** development headers installed. The latest version of GTK+ is
*** always available at http://www.gtk.org/.]))
AM_PATH_GTKGLEXT_1_0(1.0.0,,AC_MSG_ERROR([
*** gtkglext is required to build CrazyChat; please make sure you have the 
*** gtkglext development headers installed.]))

AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([assert.h arpa/inet.h fcntl.h langinfo.h libintl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset])
AC_CHECK_FUNC(inet_aton, , [AC_CHECK_LIB(resolv, inet_aton, ,
				         [AC_ERROR(inet_aton not found)])])

# Output to Makefile
AC_OUTPUT([Makefile
		src/Makefile
		])
