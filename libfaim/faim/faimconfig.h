/*
 *  faimconfig.h
 *
 * Contains various compile-time options that apply _only_ to libfaim.
 * Note that setting any of these options in a frontend header does not imply
 * that they'll get set here.  Notably, the 'debug' of this file is _not_ 
 * the same as the frontend 'debug'.  They can be different values.
 *
 */

#ifndef __FAIMCONFIG_H__
#define __FAIMCONFIG_H__

/* 
 * set debug to be > 0 if you want debugging information spewing
 * on the attached tty.  set to 0 for daily use.  this value
 * is _not_ inherited by the frontend, only this backend.
 *
 * Default: 0  
*/
#define debug 0

/*
 * Maximum number of connections the library can simultaneously
 * handle per session structure.  Five is fairly arbitrary.  
 * 
 * Normally, only one connection gets used at a time.  However, if
 * chat is used, its easily possible for several connections to be
 * open simultaneously.
 *
 * Normal connection list looks like this:
 *   1 -- used for authentication at login (closed after login)
 *   1 -- used for BOS (general messaging) (stays open for entire session)
 *   1 -- used for chatnav (room creation, etc) (opened at random)
 *  1n -- used for n connected chat rooms (AOL limits to three)
 *
 * Default: 7
 *
 */
#define AIM_CONN_MAX 7

/*
 * USE_SNAC_FOR_IMS is an old feature that allowed better
 * tracking of error messages by caching SNAC IDs of outgoing
 * ICBMs and comparing them to incoming errors.  However,
 * its a helluvalot of overhead for something that should
 * rarely happen.  
 *
 * Default: defined.  This is now defined by default
 * because it should be stable and its not too bad.  
 * And Josh wanted it.
 *
 */
#define USE_SNAC_FOR_IMS

/*
 * As of AIM 3.5 or so, AOL as added a better way of
 * logging in.  Define this to use it instead of the 
 * old Version 1.0 way.  
 *
 * The largest caveat here is that I have no idea
 * how to encode passwords using the new 3.5 way.
 * Until someone figures that out the...
 *
 * Default: Undefined.
 *
 */
#undef SNACLOGIN

/*
 * Default Authorizer server name and TCP port for the OSCAR farm.  
 *
 * You shouldn't need to change this unless you're writing
 * your own server. 
 *
 * Note that only one server is needed to start the whole
 * AIM process.  The later server addresses come from
 * the authorizer service.
 *
 * This is only here for convenience.  Its still up to
 * the client to connect to it.
 *
 */
#define FAIM_LOGIN_SERVER "login.oscar.aol.com"
#define FAIM_LOGIN_PORT 5190

/*
 * MAX_READ_ERROR can be decreased if you find dead connections
 * lingering around, and not getting detected, for too long.
 *
 * Default: 100
 *
 */
#define MAX_READ_ERROR 100

/*
 * The integer extraction/copying functions in aim_util.c have
 * both a function version and a macro version.  The macro 
 * version is suggested.  Since the function version is more
 * readable, I leave both around for reference.
 *
 * Default: defined.
 */
#define AIMUTIL_USEMACROS

/*
 * Select whether or not to use POSIX thread functionality.
 * 
 * We don't actually use threads, but we do use the POSIX mutex
 * in order to maintain thread safety.  You can use the fake locking
 * if you really don't like pthreads or you don't have it.
 *
 * Default: defined on Linux, otherwise use fake locks.
 */
#ifdef __linux__
#define FAIM_USEPTHREADS
#else
#define FAIM_USEFAKELOCKS
#endif

#endif /* __FAIMCONFIG_H__ */


