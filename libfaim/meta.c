/*
 * Administrative things for libfaim.
 *
 *  
 */

#include <aim.h>

faim_export char *aim_getbuilddate(void)
{
  return AIM_BUILDDATE;
}

faim_export char *aim_getbuildtime(void)
{
  return AIM_BUILDTIME;
}

faim_export char *aim_getbuildstring(void)
{
  static char string[100];

  snprintf(string, 99, "%d.%d.%d-%s%s", 
	   FAIM_VERSION_MAJOR,
	   FAIM_VERSION_MINOR,
	   FAIM_VERSION_MINORMINOR,
	   aim_getbuilddate(),
	   aim_getbuildtime());
  return string;
}

faim_internal void faimdprintf(struct aim_session_t *sess, int dlevel, const char *format, ...)
{
  if (!sess) {
    fprintf(stderr, "faimdprintf: no session! boo! (%d, %s)\n", dlevel, format);
    return;
  }

  if ((dlevel <= sess->debug) && sess->debugcb) {
    va_list ap;

    va_start(ap, format);
    sess->debugcb(sess, dlevel, format, ap);
    va_end(ap);
  }

  return;
}
