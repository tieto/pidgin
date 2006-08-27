

typedef struct group *Gaim__Group;

#define group perl_group

#include <glib.h>
#ifdef _WIN32
#undef pipe
#endif
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#undef group

#include "../perl-common.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "buddyicon.h"
#include "cipher.h"
#include "cmds.h"
#include "connection.h"
#include "conversation.h"
#include "debug.h"
#include "desktopitem.h"
#include "eventloop.h"
#include "ft.h"
#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkconv.h"
#include "gtkutils.h"
#include "imgstore.h"
#include "network.h"
#include "notify.h"
#include "plugin.h"
#include "pluginpref.h"
#include "pounce.h"
#include "prefs.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "roomlist.h"
#include "savedstatuses.h"
#include "server.h"
#include "signals.h"
#include "sound.h"
#include "sslconn.h"
#include "status.h"
#include "stringref.h"
/* Ewww. perl has it's own util.h which is in the include path :( */
#include "libgaim/util.h"
#include "value.h"
#include "xmlnode.h"

/* account.h */
typedef GaimAccount *			Gaim__Account;
typedef GaimAccountOption *		Gaim__Account__Option;
typedef GaimAccountUiOps *		Gaim__Account__UiOps;
typedef GaimAccountUserSplit *		Gaim__Account__UserSplit;

/* blist.h */
typedef GaimBlistNode *			Gaim__BuddyList__Node;
typedef GaimBlistNodeFlags		Gaim__BuddyList__NodeFlags;
typedef GaimBlistUiOps *		Gaim__BuddyList__UiOps;
typedef GaimBuddyList *			Gaim__BuddyList;
typedef GaimBuddy *			Gaim__BuddyList__Buddy;
typedef GaimChat *			Gaim__BuddyList__Chat;
typedef GaimContact *			Gaim__BuddyList__Contact;
typedef GaimGroup *			Gaim__BuddyList__Group;

/* buddyicon.h */
typedef GaimBuddyIcon *			Gaim__Buddy__Icon;

/* cipher.h */
typedef GaimCipher *			Gaim__Cipher;
typedef GaimCipherCaps			Gaim__CipherCaps;
typedef GaimCipherContext *		Gaim__Cipher__Context;
typedef GaimCipherOps *			Gaim__Cipher__Ops;

/* cmds.h */
typedef GaimCmdFlag			Gaim__Cmd__Flag;
typedef GaimCmdId			Gaim__Cmd__Id;
typedef GaimCmdPriority			Gaim__Cmd__Priority;
typedef GaimCmdRet			Gaim__Cmd__Ret;

/* connection.h */
typedef GaimConnection *		Gaim__Connection;
typedef GaimConnectionFlags		Gaim__ConnectionFlags;
typedef GaimConnectionState		Gaim__ConnectionState;
typedef GaimConnectionUiOps *		Gaim__Connection__UiOps;

/* conversation.h */
typedef GaimConversationType		Gaim__ConversationType;
typedef GaimUnseenState			Gaim__UnseenState;
typedef GaimConvUpdateType		Gaim__ConvUpdateType;
typedef GaimTypingState			Gaim__TypingState;
typedef GaimMessageFlags		Gaim__MessageFlags;
typedef GaimConvChatBuddyFlags		Gaim__ConvChatBuddyFlags;
typedef GaimConversation *		Gaim__Conversation;
typedef GaimConversationUiOps *		Gaim__Conversation__UiOps;
typedef GaimConvIm *			Gaim__Conversation__IM;
typedef GaimConvChat *			Gaim__Conversation__Chat;
typedef GaimConvChatBuddy *		Gaim__Conversation__ChatBuddy;

/* debug.h */
typedef GaimDebugLevel			Gaim__DebugLevel;

/* desktopitem.h */
typedef GaimDesktopItem *		Gaim__DesktopItem;
typedef GaimDesktopItemType		Gaim__DesktopItemType;

/* eventloop.h */
typedef GaimInputCondition *		Gaim__InputCondition;
typedef GaimEventLoopUiOps *		Gaim__EventLoopUiOps;

/* ft.h */
typedef GaimXfer *			Gaim__Xfer;
typedef GaimXferType			Gaim__XferType;
typedef GaimXferStatusType		Gaim__XferStatusType;
typedef GaimXferUiOps *			Gaim__XferUiOps;

/* gtkblish.h */
typedef GaimGtkBuddyList *		Gaim__GTK__BuddyList;
typedef GaimStatusIconSize		Gaim__StatusIconSize;

/* gtkutils.h */
typedef GaimButtonOrientation		Gaim__ButtonOrientation;
typedef GaimButtonStyle			Gaim__ButtonStyle;
#ifndef _WIN32
typedef GaimBrowserPlace		Gaim__BrowserPlace;
#endif /* _WIN32 */

/* gtkconv.h */
typedef GaimGtkConversation *		Gaim__GTK__Conversation;
typedef GdkPixbuf *			Gaim__GDK__Pixbuf;
typedef GtkWidget *			Gaim__GTK__Widget;

/* gtkutils.h */
typedef GtkFileSelection *		Gaim__GTK__FileSelection;
typedef GtkSelectionData *		Gaim__GTK__SelectionData;
typedef GtkTextView *			Gaim__GTK__TextView;

/* gtkconn.h */

/* imgstore.h */
typedef GaimStoredImage *		Gaim__StoredImage;

/* log.h */
typedef GaimLog *			Gaim__Log;
typedef GaimLogCommonLoggerData *	Gaim__LogCommonLoggerData;
typedef GaimLogLogger *			Gaim__Log__Logger;
typedef GaimLogReadFlags *		Gaim__Log__ReadFlags;
typedef GaimLogSet *			Gaim__LogSet;
typedef GaimLogType			Gaim__LogType;

/* network.h */
typedef GaimNetworkListenData		Gaim__NetworkListenData;
typedef GaimNetworkListenCallback	Gaim__NetworkListenCallback;

/* notify.h */
typedef GaimNotifyCloseCallback		Gaim__NotifyCloseCallback;
typedef GaimNotifyMsgType		Gaim__NotifyMsgType;
typedef GaimNotifySearchButtonType	Gaim__NotifySearchButtonType;
typedef GaimNotifySearchResults *	Gaim__NotifySearchResults;
typedef GaimNotifySearchColumn *	Gaim__NotifySearchColumn;
typedef GaimNotifySearchButton *	Gaim__NotifySearchButton;
typedef GaimNotifyType			Gaim__NotifyType;
typedef GaimNotifyUiOps *		Gaim__NotifyUiOps;

/* plugin.h */
typedef GaimPlugin *			Gaim__Plugin;
typedef GaimPluginAction *		Gaim__Plugin__Action;
typedef GaimPluginInfo *		Gaim__PluginInfo;
typedef GaimPluginLoaderInfo *		Gaim__PluginLoaderInfo;
typedef GaimPluginType			Gaim__PluginType;
typedef GaimPluginUiInfo *		Gaim__PluginUiInfo;

/* pluginpref.h */
typedef GaimPluginPref *		Gaim__PluginPref;
typedef GaimPluginPrefFrame *		Gaim__PluginPref__Frame;
typedef GaimPluginPrefType		Gaim__PluginPrefType;

/* pounce.h */
typedef GaimPounce *			Gaim__Pounce;
typedef GaimPounceEvent			Gaim__PounceEvent;

/* prefs.h */
typedef GaimPrefType			Gaim__PrefType;

/* privacy.h */
typedef GaimPrivacyType			Gaim__PrivacyType;
typedef GaimPrivacyUiOps *		Gaim__Privacy__UiOps;

/* proxy.h */
typedef GaimProxyInfo *			Gaim__ProxyInfo;
typedef GaimProxyType			Gaim__ProxyType;

/* prpl.h */
typedef GaimBuddyIconSpec *		Gaim__Buddy__Icon__Spec;
typedef GaimIconScaleRules		Gaim__IconScaleRules;
typedef GaimPluginProtocolInfo *	Gaim__PluginProtocolInfo;
typedef GaimProtocolOptions		Gaim__ProtocolOptions;

/* request.h */
typedef GaimRequestField *		Gaim__Request__Field;
typedef GaimRequestFields *		Gaim__Request__Fields;
typedef GaimRequestFieldGroup *		Gaim__Request__Field__Group;
typedef GaimRequestFieldType		Gaim__RequestFieldType;
typedef GaimRequestType			Gaim__RequestType;
typedef GaimRequestUiOps *		Gaim__Request__UiOps;

/* roomlist.h */
typedef GaimRoomlist *			Gaim__Roomlist;
typedef GaimRoomlistField *		Gaim__Roomlist__Field;
typedef GaimRoomlistFieldType		Gaim__RoomlistFieldType;
typedef GaimRoomlistRoom *		Gaim__Roomlist__Room;
typedef GaimRoomlistRoomType		Gaim__RoomlistRoomType;
typedef GaimRoomlistUiOps *		Gaim__Roomlist__UiOps;

/* savedstatuses.h */
typedef GaimSavedStatus *		Gaim__SavedStatus;
typedef GaimSavedStatusSub *		Gaim__SavedStatusSub;

/* sound.h */
typedef GaimSoundEventID		Gaim__SoundEventID;
typedef GaimSoundUiOps *		Gaim__Sound__UiOps;

/* sslconn.h */
typedef GaimInputCondition *		Gaim__Input__Condition;
typedef GaimSslConnection *		Gaim__Ssl__Connection;
typedef GaimSslErrorType		Gaim__SslErrorType;
typedef GaimSslOps *			Gaim__Ssl__Ops;

/* status.h */
typedef GaimPresence *			Gaim__Presence;
typedef GaimPresenceContext		Gaim__PresenceContext;
typedef GaimStatus *			Gaim__Status;
typedef GaimStatusAttr *		Gaim__StatusAttr;
typedef GaimStatusPrimitive		Gaim__StatusPrimitive;
typedef GaimStatusType *		Gaim__StatusType;

/* stringref.h */
typedef GaimStringref *			Gaim__Stringref;

/* util.h */
typedef GaimUtilFetchUrlData	Gaim__Util__FetchUrlData;
typedef GaimMenuAction *		Gaim__Menu__Action;

/* value.h */
typedef GaimValue *			Gaim__Value;

/* xmlnode.h */
typedef xmlnode *			Gaim__XMLNode;
typedef XMLNodeType			XMLNode__Type;

/* other.h */
