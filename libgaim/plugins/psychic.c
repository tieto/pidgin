

#include "internal.h"

#include "account.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "signals.h"
#include "status.h"
#include "version.h"

#include "plugin.h"
#include "pluginpref.h"
#include "prefs.h"


#define PLUGIN_ID       "core-psychic"
#define PLUGIN_NAME     N_("Psychic Mode")
#define PLUGIN_SUMMARY  N_("Psychic mode for incoming conversation")
#define PLUGIN_DESC     N_("Causes conversation windows to appear as other" \
			   " users begin to message you.  This works for" \
			   " AIM, ICQ, Jabber, Sametime, and Yahoo!")
#define PLUGIN_AUTHOR   "Christopher O'Brien <siege@preoccupied.net>"


#define PREFS_BASE    "/plugins/core/psychic"
#define PREF_BUDDIES  PREFS_BASE "/buddies_only"
#define PREF_NOTICE   PREFS_BASE "/show_notice"
#define PREF_STATUS   PREFS_BASE "/activate_online"
#define PREF_RAISE    PREFS_BASE "/raise_conv"


static void
buddy_typing_cb(GaimAccount *acct, const char *name, void *data) {
  GaimConversation *gconv;

  if(gaim_prefs_get_bool(PREF_STATUS) &&
     ! gaim_status_is_available(gaim_account_get_active_status(acct))) {
    gaim_debug_info("psychic", "not available, doing nothing\n");
    return;
  }

  if(gaim_prefs_get_bool(PREF_BUDDIES) &&
     ! gaim_find_buddy(acct, name)) {
    gaim_debug_info("psychic", "not in blist, doing nothing\n");
    return;
  }

  gconv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, acct);
  if(! gconv) {
    gaim_debug_info("psychic", "no previous conversation exists\n");
    gconv = gaim_conversation_new(GAIM_CONV_TYPE_IM, acct, name);

    if(gaim_prefs_get_bool(PREF_RAISE)) {
      gaim_conversation_present(gconv);
    }

    if(gaim_prefs_get_bool(PREF_NOTICE)) {

      /* This is a quote from Star Wars.  You should probably not
	 translate it literally.  If you can't find a fitting cultural
	 reference in your language, consider translating something
	 like this instead: "You feel a new message coming." */
      gaim_conversation_write(gconv, NULL,
			      _("You feel a disturbance in the force..."),
			      GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG | GAIM_MESSAGE_ACTIVE_ONLY,
			      time(NULL));
    }

    gaim_conv_im_set_typing_state(GAIM_CONV_IM(gconv), GAIM_TYPING);
  }
}


static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {

  GaimPluginPrefFrame *frame;
  GaimPluginPref *pref;
  
  frame = gaim_plugin_pref_frame_new();
  
  pref = gaim_plugin_pref_new_with_name(PREF_BUDDIES);
  gaim_plugin_pref_set_label(pref, _("Only enable for users on"
				     " the buddy list"));
  gaim_plugin_pref_frame_add(frame, pref);

  pref = gaim_plugin_pref_new_with_name(PREF_STATUS);
  gaim_plugin_pref_set_label(pref, _("Disable when away"));
  gaim_plugin_pref_frame_add(frame, pref);

  pref = gaim_plugin_pref_new_with_name(PREF_NOTICE);
  gaim_plugin_pref_set_label(pref, _("Display notification message in"
				     " conversations"));
  gaim_plugin_pref_frame_add(frame, pref);

  pref = gaim_plugin_pref_new_with_name(PREF_RAISE);
  gaim_plugin_pref_set_label(pref, _("Raise psychic conversations"));
  gaim_plugin_pref_frame_add(frame, pref);

  return frame;
}


static gboolean
plugin_load(GaimPlugin *plugin) {

  void *convs_handle;
  convs_handle = gaim_conversations_get_handle();

  gaim_signal_connect(convs_handle, "buddy-typing", plugin,
		      GAIM_CALLBACK(buddy_typing_cb), NULL);
  
  return TRUE;
}


static GaimPluginUiInfo prefs_info = {
  get_plugin_pref_frame,
  0,    /* page_num (Reserved) */
  NULL, /* frame (Reserved) */
};


static GaimPluginInfo info = {
  GAIM_PLUGIN_MAGIC,
  GAIM_MAJOR_VERSION,
  GAIM_MINOR_VERSION,
  GAIM_PLUGIN_STANDARD,   /**< type */
  NULL,                   /**< ui_requirement */
  0,                      /**< flags */
  NULL,                   /**< dependencies */
  GAIM_PRIORITY_DEFAULT,  /**< priority */
  
  PLUGIN_ID,              /**< id */
  PLUGIN_NAME,            /**< name */
  VERSION,                /**< version */
  PLUGIN_SUMMARY,         /**< summary */
  PLUGIN_DESC,            /**< description */
  PLUGIN_AUTHOR,          /**< author */
  GAIM_WEBSITE,           /**< homepage */
  
  plugin_load,            /**< load */
  NULL,                   /**< unload */
  NULL,                   /**< destroy */
  
  NULL,                   /**< ui_info */
  NULL,                   /**< extra_info */
  &prefs_info,            /**< prefs_info */
  NULL,                   /**< actions */
};


static void
init_plugin(GaimPlugin *plugin) {
  gaim_prefs_add_none(PREFS_BASE);
  gaim_prefs_add_bool(PREF_BUDDIES, FALSE);
  gaim_prefs_add_bool(PREF_NOTICE, TRUE);
  gaim_prefs_add_bool(PREF_STATUS, TRUE);
}


GAIM_INIT_PLUGIN(psychic, init_plugin, info)
