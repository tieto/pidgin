#ifndef __FAIMCONFIG_H__
#define __FAIMCONFIG_H__

/*
  faimconfig.h

  Contains various compile-time options that apply _only to the faim backend_.
  Note that setting any of these options in a frontend header does not imply
  that they'll get set here.  Notably, the 'debug' of this file is _not_ 
  the same as the frontend 'debug'.  They can be different values.

 */

/* 
   set debug to be > 0 if you want debugging information spewing
   on the attached tty.  set to 0 for daily use.  this value
   is _not_ inherited by the frontend, only this backend.

   Default: 0  
*/
#define debug 10

/*
  define TIS_TELNET_PROXY if you have a TIS firewall (Gauntlet) and
  you want to use FAIM through the firewall

  Default: undefined
 */
/* #define TIS_TELNET_PROXY "proxy.mydomain.com" */


/* #define USE_SNAC_FOR_IMS */

/* ---- these shouldn't need any changes ---- */

/* authentication server of OSCAR */
#define FAIM_LOGIN_SERVER "login.oscar.aol.com"
/* port on OSCAR authenticator to connect to */
#define FAIM_LOGIN_PORT 5190


#endif /* __FAIMCONFIG_H__ */
