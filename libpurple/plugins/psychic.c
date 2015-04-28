

#include "internal.h"

#include "account.h"
#include "buddylist.h"
#include "conversation.h"
#include "debug.h"
#include "signals.h"
#include "status.h"
#include "version.h"

#include "plugins.h"
#include "pluginpref.h"
#include "prefs.h"


#define PLUGIN_ID       "core-psychic"
#define PLUGIN_NAME     N_("Psychic Mode")
#define PLUGIN_CATEGORY N_("Utility")
#define PLUGIN_SUMMARY  N_("Psychic mode for incoming conversation")
#define PLUGIN_DESC     N_("Causes conversation windows to appear as other" \
			   " users begin to message you.  This works for" \
			   " AIM, ICQ, XMPP, Sametime, and Yahoo!")
#define PLUGIN_AUTHORS  { "Christopher O'Brien <siege@preoccupied.net>", NULL }


#define PREFS_BASE    "/plugins/core/psychic"
#define PREF_BUDDIES  PREFS_BASE "/buddies_only"
#define PREF_NOTICE   PREFS_BASE "/show_notice"
#define PREF_STATUS   PREFS_BASE "/activate_online"
#define PREF_RAISE    PREFS_BASE "/raise_conv"


static void
buddy_typing_cb(PurpleAccount *acct, const char *name, void *data) {
  PurpleIMConversation *im;

  if(purple_prefs_get_bool(PREF_STATUS) &&
     ! purple_status_is_available(purple_account_get_active_status(acct))) {
    purple_debug_info("psychic", "not available, doing nothing\n");
    return;
  }

  if(purple_prefs_get_bool(PREF_BUDDIES) &&
     ! purple_blist_find_buddy(acct, name)) {
    purple_debug_info("psychic", "not in blist, doing nothing\n");
    return;
  }

  if(FALSE == purple_account_privacy_check(acct, name)) {
    purple_debug_info("psychic", "user %s is blocked\n", name);
    return;
  }

  im = purple_conversations_find_im_with_account(name, acct);
  if(! im) {
    purple_debug_info("psychic", "no previous conversation exists\n");
    im = purple_im_conversation_new(acct, name);

    if(purple_prefs_get_bool(PREF_RAISE)) {
      purple_conversation_present(PURPLE_CONVERSATION(im));
    }

    if(purple_prefs_get_bool(PREF_NOTICE)) {

      /* This is a quote from Star Wars.  You should probably not
	 translate it literally.  If you can't find a fitting cultural
	 reference in your language, consider translating something
	 like this instead: "You feel a new message coming." */
      purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
		_("You feel a disturbance in the force..."),
		PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_ACTIVE_ONLY);
    }

    /* Necessary because we may be creating a new conversation window. */
    purple_im_conversation_set_typing_state(im, PURPLE_IM_TYPING);
  }
}


static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {

  PurplePluginPrefFrame *frame;
  PurplePluginPref *pref;

  frame = purple_plugin_pref_frame_new();

  pref = purple_plugin_pref_new_with_name(PREF_BUDDIES);
  purple_plugin_pref_set_label(pref, _("Only enable for users on"
				     " the buddy list"));
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_name(PREF_STATUS);
  purple_plugin_pref_set_label(pref, _("Disable when away"));
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_name(PREF_NOTICE);
  purple_plugin_pref_set_label(pref, _("Display notification message in"
				     " conversations"));
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_name(PREF_RAISE);
  purple_plugin_pref_set_label(pref, _("Raise psychic conversations"));
  purple_plugin_pref_frame_add(frame, pref);

  return frame;
}


static PurplePluginInfo *
plugin_query(GError **error) {

  const gchar * const authors[] = PLUGIN_AUTHORS;

  return purple_plugin_info_new(
    "id",             PLUGIN_ID,
    "name",           PLUGIN_NAME,
    "version",        DISPLAY_VERSION,
    "category",       PLUGIN_CATEGORY,
    "summary",        PLUGIN_SUMMARY,
    "description",    PLUGIN_DESC,
    "authors",        authors,
    "website",        PURPLE_WEBSITE,
    "abi-version",    PURPLE_ABI_VERSION,
    "pref-frame-cb",  get_plugin_pref_frame,
    NULL
  );
}


static gboolean
plugin_load(PurplePlugin *plugin, GError **error) {

  void *convs_handle;

  purple_prefs_add_none(PREFS_BASE);
  purple_prefs_add_bool(PREF_BUDDIES, FALSE);
  purple_prefs_add_bool(PREF_NOTICE, TRUE);
  purple_prefs_add_bool(PREF_STATUS, TRUE);

  convs_handle = purple_conversations_get_handle();

  purple_signal_connect(convs_handle, "buddy-typing", plugin,
		      PURPLE_CALLBACK(buddy_typing_cb), NULL);

  return TRUE;
}


static gboolean
plugin_unload(PurplePlugin *plugin, GError **error) {

  return TRUE;
}


PURPLE_PLUGIN_INIT(psychic, plugin_query, plugin_load, plugin_unload);
