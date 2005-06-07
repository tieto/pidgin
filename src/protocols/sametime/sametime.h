

/* CFLAGS trumps configure values */


/** default active message */
#ifndef MW_PLUGIN_DEFAULT_ACTIVE_MSG
#define MW_PLUGIN_DEFAULT_ACTIVE_MSG  "Talk to me"
#endif
/* "Talk to me" */


/** default away message */
#ifndef MW_PLUGIN_DEFAULT_AWAY_MSG
#define MW_PLUGIN_DEFAULT_AWAY_MSG  "Not here right now"
#endif
/* "Not here right now" */


/** default busy message */
#ifndef MW_PLUGIN_DEFAULT_BUSY_MSG
#define MW_PLUGIN_DEFAULT_BUSY_MSG  "Please do not disturb me"
#endif
/* "Please do not disturb me" */


/** default host for the gaim plugin. You can specialize a build to
    default to your server by supplying this at compile time */
#ifndef MW_PLUGIN_DEFAULT_HOST
#define MW_PLUGIN_DEFAULT_HOST  ""
#endif
/* "" */


/** default port for the gaim plugin. You can specialize a build to
    default to your server by supplying this at compile time */
#ifndef MW_PLUGIN_DEFAULT_PORT
#define MW_PLUGIN_DEFAULT_PORT  1533
#endif
/* 1533 */


/** client id to report to the server. See mwLoginType in meanwhile's
    mw_common.h for some sample values */
#ifndef MW_CLIENT_TYPE_ID
#define MW_CLIENT_TYPE_ID  mwLogin_MEANWHILE
#endif
/*  mwLogin_MEANWHILE */


