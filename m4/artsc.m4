dnl Test for the ArtsC interface.

AC_DEFUN([AM_PATH_ARTSC],
[
AC_ARG_ENABLE(artsc-test,
[  --disable-artsc-test      Do not compile and run a test ArtsC program.],
[artsc_test="yes"],
[artsc_test="no"])

dnl Search for the arts-config program.
AC_PATH_PROG(ARTSC_CONFIG, artsc-config, no)
min_artsc_version=ifelse([$1],,0.9.5,$1)
AC_MSG_CHECKING(for ArtsC - version >= $min_artsc_version)

no_artsc=""
if test "x$ARTSC_CONFIG" != "xno"; then
  ARTSC_CFLAGS=`$ARTSC_CONFIG --cflags`
  ARTSC_LIBS=`$ARTSC_CONFIG --libs`

  artsc_major_version=`$ARTSC_CONFIG $artsc_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
  artsc_minor_version=`$ARTSC_CONFIG $artsc_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
  artsc_micro_version=`$ARTSC_CONFIG $artsc_config_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

  # Test if a simple ArtsC program can be created.
  if test "x$artsc_test" = "xyes"; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $ARTSC_CFLAGS"
    LIBS="$LIBS $ARTSC_LIBS"

    rm -f conf.artsc
    AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <artsc.h>

char*
my_strdup(char *str)
{
  char *new_str;
 
  if (str) {
    new_str = malloc((strlen(str) + 1) * sizeof(char));
    strcpy(new_str, str);
  } else {
    new_str = NULL;
  }

  return new_str;
}
 
int main()
{
  int major, minor, micro;
  char *tmp_version;
 
  system ("touch conf.artsc");

  /* HP/UX 9 writes to sscanf strings */
  tmp_version = my_strdup("$min_artsc_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_artsc_version");
     exit(1);
  }
 
  if (($artsc_major_version > major) ||
     (($artsc_major_version == major) && ($artsc_minor_version > minor)) ||
     (($artsc_major_version == major) && ($artsc_minor_version == minor)
                                    && ($artsc_micro_version >= micro))) {
    return 0;
  } else {
    printf("\n*** 'artsc-config --version' returned %d.%d.%d, but the minimum version\n", $artsc_major_version, $artsc_minor_version, $artsc_micro_version);
    printf("*** of ARTSC required is %d.%d.%d. If artsc-config is correct, then it is\n", major, minor, micro);
    printf("*** best to upgrade to the required version.\n");
    printf("*** If artsc-config was wrong, set the environment variable ARTSC_CONFIG\n");
    printf("*** to point to the correct copy of artsc-config, and remove the file\n");
    printf("*** config.cache before re-running configure\n");
    return 1;
  }
}
],, no_artsc=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
   CFLAGS="$ac_save_CFLAGS"
   LIBS="$ac_save_LIBS"
  fi
else
  no_artsc=yes
fi

if test "x$no_artsc" != "xyes"; then
  AC_MSG_RESULT(yes)
  ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(no)

  if test "x$ARTSC_CONFIG" = "xno" ; then
    echo "*** The artsc-config script installed by ArtsC could not be found"
    echo "*** If ArtsC was installed in PREFIX, make sure PREFIX/bin is in"
    echo "*** your path, or set the ARTSC_CONFIG environment variable to the"
    echo "*** full path to artsc-config."
  else
    if test -f conf.artsc ; then
      :
    else
      echo "*** Could not run ArtsC test program, checking why..."
      CFLAGS="$CFLAGS $ARTSC_CFLAGS"
      LIBS="$LIBS $ARTSC_LIBS"
      AC_TRY_LINK([
#include <stdio.h>
#include <artsc.h>
],[return 0;],
[ echo "*** The test program compiled, but did not run. This usually means"
  echo "*** that the run-time linker is not finding ArtsC or finding the wrong"
  echo "*** version of ArtsC. If it is not finding ArtsC, you'll need to set your"
  echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
  echo "*** to the installed location  Also, make sure you have run ldconfig if that"
  echo "*** is required on your system"
  echo "***"
  echo "*** If you have an old version installed, it is best to remove it, although"
  echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
  [ echo "*** The test program failed to compile or link. See the file config.log for the"
  echo "*** exact error that occured. This usually means ArtsC was incorrectly installed"
  echo "*** or that you have moved ArtsC since it was installed. In the latter case, you"
  echo "*** may want to edit the artsc-config script: $ARTSC_CONFIG" ])
      CFLAGS="$ac_save_CFLAGS"
      LIBS="$ac_save_LIBS"
    fi
  fi
  ARTSC_CFLAGS=""
  ARTSC_LIBS=""
  ifelse([$3], , :, [$3])
fi

AC_SUBST(ARTSC_CFLAGS)
AC_SUBST(ARTSC_LIBS)
rm -f conf.artsc

])
