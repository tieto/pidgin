dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/main.c)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([gaim], [0.63])

AC_PREREQ([2.50])

AC_PATH_PROG(sedpath, sed)

dnl Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AM_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL


ALL_LINGUAS="am bg ca cs da de es fi fr hu it ja ko nl pl ro ru sk sr sv zh_CN zh_TW"
AM_GNU_GETTEXT_VERSION(0.10.40)
AM_GNU_GETTEXT

AC_CYGWIN


dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h locale.h signal.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_STRUCT_TM

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(strdup strstr atexit)
dnl Checks for getopt in standard library
AC_CHECK_FUNCS(getopt_long , ,
AC_LIBOBJ(getopt)
AC_LIBOBJ(getopt1)) 
 

dnl Check for inet_aton
AC_CHECK_FUNC(inet_aton, , [AC_CHECK_LIB(resolv, inet_aton, , 
				         [AC_ERROR(inet_aton not found)])])
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

dnl This is a bad, bad hack.  I am a bad, bad man.
CFLAGS="$CFLAGS -I/opt/include -I\$(top_srcdir)"

AC_ARG_ENABLE(distrib,,,enable_distrib=no)
AM_CONDITIONAL(DISTRIB, test "x$enable_distrib" = "xyes")
AC_ARG_ENABLE(multi,   [  --disable-multi         disable multiple connections],,enable_multi=yes)
AC_ARG_ENABLE(prpls,   [  --disable-prpls         don't build dynamic protocol plugins],,enable_prpls=yes)
AC_ARG_WITH(static-prpls,    [  --with-static-prpls     link in certain protocols statically],[STATIC_PRPLS=`echo $withval | $sedpath 's/,/ /g'`],STATIC_PRPLS="")
if test "x$STATIC_PRPLS" = "xall" ; then
	STATIC_PRPLS="gg irc jabber msn napster oscar toc yahoo zephyr"
fi
AC_SUBST(STATIC_PRPLS)
STATIC_LINK_LIBS=
extern_init=
load_proto=
for i in $STATIC_PRPLS ; do
	STATIC_LINK_LIBS="$STATIC_LINK_LIBS protocols/$i/lib$i.a"
	extern_init="$extern_init extern void ${i}_init(struct prpl *);"
	load_proto="$load_proto load_protocol(${i}_init);"
	case $i in
		gg) static_gg=yes ;;
		irc) static_irc=yes ;;
		jabber) static_jabber=yes ;;
		msn) static_msn=yes ;;
		napster) static_napster=yes ;;
		oscar) static_oscar=yes ;;
		toc) static_toc=yes ;;
		yahoo) static_yahoo=yes ;;
		zephyr) static_zephyr=yes ;;
		*) echo "Invalid static protocol $i!!" ; exit ;;
	esac
done
AM_CONDITIONAL(STATIC_GG, test "x$static_gg" = "xyes")
AM_CONDITIONAL(STATIC_IRC, test "x$static_irc" = "xyes")
AM_CONDITIONAL(STATIC_JABBER, test "x$static_jabber" = "xyes")
AM_CONDITIONAL(STATIC_MSN, test "x$static_msn" = "xyes")
AM_CONDITIONAL(STATIC_NAPSTER, test "x$static_napster" = "xyes")
AM_CONDITIONAL(STATIC_OSCAR, test "x$static_oscar" = "xyes")
AM_CONDITIONAL(STATIC_TOC, test "x$static_toc" = "xyes")
AM_CONDITIONAL(STATIC_YAHOO, test "x$static_yahoo" = "xyes")
AM_CONDITIONAL(STATIC_ZEPHYR, test "x$static_zephyr" = "xyes")
AC_SUBST(STATIC_LINK_LIBS)
AC_DEFINE_UNQUOTED(STATIC_PROTO_INIT, $extern_init void static_proto_init() { $load_proto },
	[Loads static protocol plugin module initialization functions.])

AC_ARG_ENABLE(audio,   [  --disable-audio         compile without libao/libaudiofile for sound playing],,enable_audio=yes)
AC_ARG_ENABLE(nas,     [  --enable-nas            enable NAS (Network Audio System) support],,enable_nas=no)
AC_ARG_ENABLE(plugins, [  --disable-plugins       compile without plugin support],,enable_plugins=yes)
AC_ARG_ENABLE(perl,    [  --disable-perl          compile without perl scripting],,enable_perl=yes)
AC_ARG_ENABLE(gtkspell, [  --disable-gtkspell      compile without GtkSpell automatic spell checking],,enable_gtkspell=yes)
AC_ARG_ENABLE(debug,   [  --enable-debug          compile with debugging support],,enable_debug=no)
AC_ARG_ENABLE(screensaver,   [  --disable-screensaver   compile without X screensaver extension],,enable_xss=yes)
AC_ARG_ENABLE(sm,      [  --disable-sm            compile without X session management support],,enable_sm=yes)
AC_ARG_WITH(krb4,      [  --with-krb4=PREFIX      Compile Zephyr plugin with Kerberos 4 support],kerberos="$withval",kerberos="no")

if test "$enable_debug" = yes ; then
	CFLAGS="$CFLAGS -Wall -g3"
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
fi

AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR([
*** GLib 2.0 is required to build Gaim; please make sure you have the GLib
*** development headers installed. The latest version of GLib is
*** always available at http://www.gtk.org/.]))
AM_PATH_GTK_2_0(2.0.0,,AC_MSG_ERROR([
*** GTK+ 2.0 is required to build Gaim; please make sure you have the GTK+
*** development headers installed. The latest version of GTK+ is
*** always available at http://www.gtk.org/.]))

AC_PATH_PROG(gaimpath, gaim)
CFLAGS="$CFLAGS $GTK_CFLAGS -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"

AC_PATH_XTRA
# We can't assume that $x_libraries will be set, because autoconf does not
# set it in the case when the X libraries are in a standard place.
# Ditto for $x_includes
if test X"$x_libraries" = X"" ; then
	x_libpath_add=
else
	x_libpath_add="-L$x_libraries"
fi
if test X"$x_includes" = X"" ; then
	x_incpath_add=
else
	x_incpath_add="-I$x_includes"
fi

dnl Check for XScreenSaver
if test "x$enable_xss" = "xyes" ; then
	old_LIBS="$LIBS"
	LIBS="$LIBS $GTK_LIBS $x_libpath_add"
	XSS_LIBS="no"
	XSS_HEADERS="no"
	AC_CHECK_LIB(Xext, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_EXTRA_LIBS"],[],[-lX11 -lXext -lm])
	AC_CHECK_LIB(Xss, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_LIBS $X_EXTRA_LIBS -lXss"],[],[-lX11 -lXext -lm])
	if test \! "$XSS_LIBS" = "no"; then
		AC_TRY_COMPILE([
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>
				],[],[
				AC_DEFINE(USE_SCREENSAVER, 1, [Define if we're using XScreenSaver.])],[enable_xss=no]
				)
	else
	       XSS_LIBS=""
	       enable_xss=no
	fi
	LIBS="$old_LIBS"
else
	XSS_LIBS=""
	enable_xss=no
fi
AC_SUBST(XSS_LIBS)


dnl Check for X session management libs
if test "x$enable_sm" = "xyes"; then
	enable_sm=no
	AC_CHECK_LIB(SM, SmcSaveYourselfDone, found_sm_lib=true, , [$x_libpath_add -lICE])
	if test "$found_sm_lib" = "true"; then
		AC_CHECK_HEADERS(X11/SM/SMlib.h, SM_LIBS="$x_libpath_add -lSM -lICE" enable_sm=yes)
	fi
else
	SM_LIBS=""
	enable_sm=no
fi
AC_SUBST(SM_LIBS)
if test "$enable_sm" = "yes"; then
	AC_DEFINE(USE_SM, 1, [Define if we're using X Session Management.])
fi


AC_DEFUN(GC_TM_GMTOFF,
[AC_REQUIRE([AC_STRUCT_TM])dnl
AC_CACHE_CHECK([for tm_gmtoff in struct tm], ac_cv_struct_tm_gmtoff,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <$ac_cv_struct_tm>], [struct tm tm; tm.tm_gmtoff;],
  ac_cv_struct_tm_gmtoff=yes, ac_cv_struct_tm_gmtoff=no)])
if test "$ac_cv_struct_tm_gmtoff" = yes; then
  AC_DEFINE(HAVE_TM_GMTOFF, 1, [tm_gmtoff is available.])
fi
])

GC_TM_GMTOFF



dnl This was taken straight from X-Chat.
dnl X-Chat is the greatest application ever, not only
dnl because it's a rocking IRC client but also because
dnl it's very easy to learn from.
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

		oldLIBS=$LIBS
		LIBS="$LIBS $PERL_LIBS"
		AC_MSG_CHECKING(for libperl)
		AC_CHECK_FUNCS(perl_run, [], enable_perl=no)
		LIBS=$oldLIBS
	fi
fi

if test "$enable_perl" = yes ; then
	AC_SUBST(PERL_CFLAGS)
	AC_SUBST(PERL_LIBS)
	AM_CONDITIONAL(USE_PERL, test "x$enable_perl" = "xyes")

	dnl This is almost definitely wrong, but in case there's
	dnl something I'm missing, I'll leave it in.
	AC_CHECK_FUNCS(Perl_eval_pv)

	AC_MSG_CHECKING(for old perl)
	PERL_OLD=`$perlpath -e 'if($]<5.006){printf"yes\n";}else{printf"no\n";}'`

	if test "$PERL_OLD" = "yes"; then
		AC_DEFINE(OLD_PERL, 1, [Define if old perl is installed.])
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
else
	PERL_CFLAGS=
	PERL_LIBS=
	AM_CONDITIONAL(USE_PERL, false)
fi

dnl Thanks, Evan.
if test "$enable_gtkspell" = yes ; then
	PKG_CHECK_MODULES(GTKSPELL, gtkspell-2.0 >= 2.0.2, , enable_gtkspell=no)
	if test "$enable_gtkspell" = "yes" ; then
		AC_SUBST(GTKSPELL_CFLAGS)
		AC_SUBST(GTKSPELL_LIBS)
        	AC_DEFINE(USE_GTKSPELL,,[do we have gtkspell?])
	fi
fi

if test "$enable_nas" = yes ; then
	AC_DEFINE(USE_NAS_AUDIO, 1, [Define if we have NAS sound support.])
	SOUND_LIBS="$SOUND_LIBS -laudio -lXt"
fi

if test "$enable_audio" = yes ; then
	GAIM_PATH_AO(found_ao_lib=true)

	AM_PATH_AUDIOFILE([0.2.0], found_af_lib=true)

	if test "$found_ao_lib" = "true" -a "$found_af_lib" = "true"; then
		SOUND_LIBS="$SOUND_LIBS $AO_LIBS $AUDIOFILE_LIBS"
		AC_SUBST(SOUND_LIBS)
		AC_DEFINE(USE_AO, 1, [Define if we're using libao and libaudiofile for sound playing])
		enable_audio=yes
	else
		enable_audio=no
	fi
else
	enable_audio=no
fi

if test "$ac_cv_cygwin" = yes ; then
	LDADD="$LDADD -static"
	CFLAGS="$CFLAGS -Wall -g"
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
fi                           

AC_SUBST(CFLAGS)
AC_SUBST(LDADD)
AC_SUBST(LIBS)

if test "x$enable_multi" != "xyes" ; then
	AC_DEFINE(NO_MULTI, 1, [Define if multiple connections is disabled.])
	enable_multi=no
fi

if test "x$enable_plugins" = "xyes" ; then
	AC_DEFINE(GAIM_PLUGINS, 1, [Define if plugins are enabled.])
	AM_CONDITIONAL(PLUGINS, test "x$enable_plugins" = "xyes")
else
	AM_CONDITIONAL(PLUGINS, false)
	enable_plugins=no
	enable_prpls=no
fi

if test "x$enable_prpls" = "xyes" ; then
	AM_CONDITIONAL(PRPLS, test "x$enable_plugins" = "xyes")
else
	AM_CONDITIONAL(PRPLS, false)
	enable_prpls=no
fi

dnl checks for icqlib
AC_CHECK_HEADERS(asm/byteorder.h byteswap.h endian.h machine/endian.h arpa/nameser_compat.h)
AC_CHECK_FUNCS(bswap_32 bswap_16)
AC_C_BIGENDIAN

dnl checks for jabber
dnl AC_CHECK_SIZEOF(short)
AC_CHECK_FUNCS(snprintf connect)

dnl checks for zephyr
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
AC_CHECK_FUNCS(gethostid lrand48)
AC_CHECK_FUNCS(memcpy memmove random strchr strerror vprintf)
AC_CHECK_HEADERS(malloc.h paths.h sgtty.h stdarg.h sys/cdefs.h)
AC_CHECK_HEADERS(sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h)
AC_CHECK_HEADERS(sys/select.h sys/uio.h sys/utsname.h sys/wait.h)
AC_CHECK_HEADERS(termios.h)
AC_VAR_TIMEZONE_EXTERNALS

AC_OUTPUT([Makefile
	       Doxyfile
           doc/Makefile
	   intl/Makefile
           pixmaps/Makefile
	   pixmaps/protocols/Makefile
           pixmaps/protocols/msn/Makefile
           pixmaps/protocols/yahoo/Makefile
	   pixmaps/protocols/oscar/Makefile
	   pixmaps/protocols/napster/Makefile
	   pixmaps/protocols/jabber/Makefile
	   pixmaps/protocols/irc/Makefile
	   pixmaps/protocols/icq/Makefile
	   pixmaps/protocols/gg/Makefile
	   pixmaps/smileys/Makefile
	   pixmaps/smileys/default/Makefile
	   pixmaps/status/Makefile
	   pixmaps/status/default/Makefile
	   plugins/Makefile
	   plugins/docklet/Makefile
	   plugins/gestures/Makefile
	   plugins/perl/Makefile
	   plugins/ticker/Makefile
	   po/Makefile.in 
           sounds/Makefile
	   src/Makefile
	   src/protocols/Makefile
	   src/protocols/gg/Makefile
	   src/protocols/icq/Makefile
	   src/protocols/irc/Makefile
	   src/protocols/jabber/Makefile
	   src/protocols/msn/Makefile
	   src/protocols/napster/Makefile
	   src/protocols/oscar/Makefile
	   src/protocols/toc/Makefile
	   src/protocols/yahoo/Makefile
	   src/protocols/zephyr/Makefile
	   gaim.spec
          ])

echo
echo $PACKAGE $VERSION

echo
echo Allow Multiple Connections.... : $enable_multi
echo Build Protocol Plugins........ : $enable_prpls
echo Protocols to link statically.. : $STATIC_PRPLS
echo
echo UI Library.................... : GTK 2.x
echo
echo Build with Plugin support..... : $enable_plugins
echo Build with Perl support....... : $enable_perl
echo Build with Audio support...... : $enable_audio
echo Build with NAS support........ : $enable_nas
echo Build with GtkSpell support... : $enable_gtkspell
echo
echo Use XScreenSaver Extension.... : $enable_xss
echo Use X Session Management...... : $enable_sm
echo
echo Print debugging messages...... : $enable_debug
echo
eval eval echo Gaim will be installed in $bindir.
if test "x$gaimpath" != "x" ; then
	echo Warning: You have an old copy of gaim at $gaimpath.
fi
echo
echo configure complete, now type \'make\'
echo
