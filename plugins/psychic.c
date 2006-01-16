

#include "internal.h"

#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "signals.h"
#include "version.h"

#include "plugin.h"
#include "pluginpref.h"
#include "prefs.h"


#define DEBUG_INFO(a...) gaim_debug_info("psychic", a)


#define PLUGIN_ID       "core-psychic"
#define PLUGIN_NAME     N_("Psychic Mode")
#define PLUGIN_SUMMARY  N_("Psychic mode for incoming conversation")
#define PLUGIN_DESC     N_("Causes conversation windows to appear as other" \
			   " users begin to message you")
#define PLUGIN_AUTHOR   "Christopher O'Brien <siege@preoccupied.net>"


#define PREFS_BASE    "/plugins/core/psychic"
#define PREF_BUDDIES  PREFS_BASE "/buddies_only"
#define PREF_NOTICE   PREFS_BASE "/show_notice"


static void
buddy_typing_cb(GaimAccount *acct, const char *name, void *data) {
  GaimConversation *gconv;

  if(gaim_prefs_get_bool(PREF_BUDDIES)) {
    if(! gaim_find_buddy(acct, name)) {
      DEBUG_INFO("not in blist, doing nothing\n");
      return;
    }
  }

  gconv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, acct);
  if(! gconv) {
    DEBUG_INFO("no previous conversation exists\n");
    gconv = gaim_conversation_new(GAIM_CONV_TYPE_IM, acct, name);
    gaim_conversation_present(gconv);

    if(gaim_prefs_get_bool(PREF_NOTICE)) {
      gaim_conversation_write(gconv, NULL,
			      _("You feel a disturbance in the force..."),
			      GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
			      time(NULL));
    }
  }
}


static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {

  GaimPluginPrefFrame *frame;
  GaimPluginPref *pref;
  
  frame = gaim_plugin_pref_frame_new();
  
  pref = gaim_plugin_pref_new_with_name(PREF_BUDDIES);
  gaim_plugin_pref_set_label(pref, _("Only enable for users on the buddy list"));
  gaim_plugin_pref_frame_add(frame, pref);

  pref = gaim_plugin_pref_new_with_name(PREF_NOTICE);
  gaim_plugin_pref_set_label(pref, _("Display notification message in"
				     " conversations"));
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
}


GAIM_INIT_PLUGIN(psychic, init_plugin, info)
