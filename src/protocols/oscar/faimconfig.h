/*
 *  faimconfig.h
 *
 * Contains various compile-time options that apply _only_ to libfaim.
 *
 */

#ifndef __FAIMCONFIG_H__
#define __FAIMCONFIG_H__

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
 * What type of synchronisation to use.
 * 
 * We don't actually use threads, but can use the POSIX mutex
 * in order to maintain thread safety.  You can use the fake locking
 * if you really don't like pthreads (which I don't) or if you don't
 * have it.
 *
 *   USEPTHREADS - Use POSIX mutecies
 *   USEFAKELOCKS - Use little stub spinners to help find locking bugs
 *   USENOPLOCKS - No-op out all synchro calls at compile time
 * 
 * Default: use noplocks by default.
 *
 * !!!NOTE: Even with USEPTHREADS turned on, libfaim is not fully thread
 *          safe.  It will still take some effort to add locking calls to
 *          the places that need them.  In fact, this feature in general
 *          is in danger of being officially deprecated and removed from 
 *          the code.
 *
 */
#undef FAIM_USEPTHREADS
#undef FAIM_USEFAKELOCKS
#define FAIM_USENOPLOCKS

/*
 * Size of the SNAC caching hash.
 *
 * Default: 16
 *
 */
#define FAIM_SNAC_HASH_SIZE 16

/*
 * If building on Win32, define WIN32_STATIC if you don't want
 * to compile libfaim as a DLL (and instead link it right into
 * your app).
 */
#define WIN32_STATIC

#endif /* __FAIMCONFIG_H__ */


