/* Allow the Perl code to see deprecated functions, so we can continue to
 * export them to Perl plugins. */
#undef PURPLE_DISABLE_DEPRECATED

typedef struct group *Purple__Group;

#define group perl_group

#include <glib.h>
#ifdef _WIN32
#undef pipe
#undef STRINGIFY
#endif
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#undef group

#include "../perl-common.h"

#include "accounts.h"
#include "accountopt.h"
#include "buddylist.h"
#include "buddyicon.h"
#include "certificate.h"
#include "cipher.h"
#include "ciphers/aescipher.h"
#include "ciphers/des3cipher.h"
#include "ciphers/descipher.h"
#include "ciphers/hmaccipher.h"
#include "ciphers/pbkdf2cipher.h"
#include "ciphers/rc4cipher.h"
#include "hash.h"
#include "ciphers/md4hash.h"
#include "ciphers/md5hash.h"
#include "ciphers/sha1hash.h"
#include "ciphers/sha256hash.h"
#include "cmds.h"
#include "connection.h"
#include "conversations.h"
#include "core.h"
#include "debug.h"
#include "desktopitem.h"
#include "eventloop.h"
#include "ft.h"
#ifdef PURPLE_GTKPERL
#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkconv.h"
#include "gtkutils.h"
#endif
#include "idle.h"
#include "imgstore.h"
#include "network.h"
#include "notify.h"
#include "plugin.h"
#include "pluginpref.h"
#include "pounce.h"
#include "prefs.h"
#include "presences.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "roomlist.h"
#include "savedstatuses.h"
#include "server.h"
#include "signals.h"
#include "smiley.h"
#include "sound.h"
#include "sslconn.h"
#include "status.h"
#include "stringref.h"
/* Ewww. perl has it's own util.h which is in the include path :( */
#include "libpurple/util.h"
#include "whiteboard.h"
#include "xmlnode.h"

/* account.h */
typedef PurpleAccount *			Purple__Account;
typedef PurpleAccountOption *		Purple__Account__Option;
typedef PurpleAccountUserSplit *		Purple__Account__UserSplit;
typedef PurpleAccountPrivacyType		Purple__Account__PrivacyType;

/* buddylist.h */
typedef PurpleBListNode *			Purple__BuddyList__Node;
typedef PurpleCountingNode *			Purple__BuddyList__CountingNode;
typedef PurpleBuddyList *			Purple__BuddyList;
typedef PurpleBuddy *			Purple__BuddyList__Buddy;
typedef PurpleChat *			Purple__BuddyList__Chat;
typedef PurpleContact *			Purple__BuddyList__Contact;
typedef PurpleGroup *			Purple__BuddyList__Group;

/* buddyicon.h */
typedef PurpleBuddyIcon *			Purple__Buddy__Icon;

/* certificate.h */
typedef PurpleCertificate *			Purple__Certificate;
typedef PurpleCertificatePool *			Purple__Certificate__Pool;
typedef PurpleCertificateScheme *		Purple__Certificate__Scheme;
typedef PurpleCertificateVerifier *		Purple__Certificate__Verifier;
typedef PurpleCertificateVerificationRequest *	Purple__Certificate__VerificationRequest;
typedef PurpleCertificateVerificationStatus	Purple__Certificate__VerificationStatus;

/* cipher.h */
typedef PurpleCipher *			Purple__Cipher;
typedef PurpleCipherBatchMode		Purple__Cipher__BatchMode;

/* cmds.h */
typedef PurpleCmdFlag			Purple__Cmd__Flag;
typedef PurpleCmdId			Purple__Cmd__Id;
typedef PurpleCmdPriority			Purple__Cmd__Priority;
typedef PurpleCmdRet			Purple__Cmd__Ret;

/* connection.h */
typedef PurpleConnection *		Purple__Connection;
typedef PurpleConnectionFlags		Purple__ConnectionFlags;
typedef PurpleConnectionState		Purple__ConnectionState;

/* conversations.h */
typedef PurpleMessageFlags			Purple__MessageFlags;
typedef PurpleConversation *		Purple__Conversation;
typedef PurpleConversationUpdateType		Purple__Conversation__UpdateType;
typedef PurpleIMConversation *			Purple__IMConversation;
typedef PurpleIMTypingState		Purple__IMTypingState;
typedef PurpleChatConversation *		Purple__ChatConversation;
typedef PurpleChatUser *	Purple__ChatUser;
typedef PurpleChatUserFlags	Purple__ChatUser__Flags;

/* core.h */

typedef PurpleCore *			Purple__Core;

/* debug.h */
typedef PurpleDebugLevel			Purple__DebugLevel;

/* desktopitem.h */
typedef PurpleDesktopItem *		Purple__DesktopItem;
typedef PurpleDesktopItemType		Purple__DesktopItemType;

/* eventloop.h */
typedef PurpleInputCondition *		Purple__InputCondition;

/* ft.h */
typedef PurpleXfer *			Purple__Xfer;
typedef PurpleXferType			Purple__XferType;
typedef PurpleXferStatusType		Purple__XferStatusType;


#ifdef PURPLE_GTKPERL
/* gtkblish.h */
typedef PurpleGtkBuddyList *		Purple__GTK__BuddyList;
typedef PurpleStatusIconSize		Purple__StatusIconSize;

/* gtkutils.h */
typedef PurpleButtonOrientation		Purple__ButtonOrientation;
typedef PurpleButtonStyle			Purple__ButtonStyle;
#ifndef _WIN32
typedef PurpleBrowserPlace		Purple__BrowserPlace;
#endif /* _WIN32 */

/* gtkconv.h */
typedef PurpleUnseenState			Purple__UnseenState;
typedef PurpleGtkConversation *		Purple__GTK__Conversation;
typedef GdkPixbuf *			Purple__GDK__Pixbuf;
typedef GtkWidget *			Purple__GTK__Widget;

/* gtkutils.h */
typedef GtkFileSelection *		Purple__GTK__FileSelection;
typedef GtkSelectionData *		Purple__GTK__SelectionData;
typedef GtkTextView *			Purple__GTK__TextView;

/* gtkconn.h */
#endif

/* hash.h */
typedef PurpleHash *			Purple__Hash;

/* imgstore.h */
typedef PurpleStoredImage *		Purple__StoredImage;

/* log.h */
typedef PurpleLog *			Purple__Log;
typedef PurpleLogCommonLoggerData *	Purple__LogCommonLoggerData;
typedef PurpleLogLogger *			Purple__Log__Logger;
typedef PurpleLogReadFlags *		Purple__Log__ReadFlags;
typedef PurpleLogSet *			Purple__LogSet;
typedef PurpleLogType			Purple__LogType;

/* network.h */
typedef PurpleNetworkListenData *		Purple__NetworkListenData;
typedef PurpleNetworkListenCallback	Purple__NetworkListenCallback;

/* notify.h */
typedef PurpleNotifyCloseCallback		Purple__NotifyCloseCallback;
typedef PurpleNotifyMsgType		Purple__NotifyMsgType;
typedef PurpleNotifySearchButtonType	Purple__NotifySearchButtonType;
typedef PurpleNotifySearchResults *	Purple__NotifySearchResults;
typedef PurpleNotifySearchColumn *	Purple__NotifySearchColumn;
typedef PurpleNotifySearchButton *	Purple__NotifySearchButton;
typedef PurpleNotifyType			Purple__NotifyType;
typedef PurpleNotifyUserInfo *	Purple__NotifyUserInfo;
typedef PurpleNotifyUserInfoEntry *	Purple__NotifyUserInfoEntry;

/* plugin.h */
typedef PurplePlugin *			Purple__Plugin;
typedef PurplePluginAction *		Purple__Plugin__Action;
typedef PurplePluginInfo *		Purple__PluginInfo;
typedef PurplePluginLoaderInfo *		Purple__PluginLoaderInfo;
typedef PurplePluginType			Purple__PluginType;
typedef PurplePluginUiInfo *		Purple__PluginUiInfo;

/* pluginpref.h */
typedef PurplePluginPref *		Purple__PluginPref;
typedef PurplePluginPrefFrame *		Purple__PluginPref__Frame;
typedef PurplePluginPrefType		Purple__PluginPrefType;
typedef PurpleStringFormatType		Purple__String__Format__Type;

/* pounce.h */
typedef PurplePounce *			Purple__Pounce;
typedef PurplePounceEvent			Purple__PounceEvent;

/* prefs.h */
typedef PurplePrefType			Purple__PrefType;

/* presence.h */
typedef PurplePresence *		Purple__Presence;
typedef PurpleAccountPresence *		Purple__AccountPresence;
typedef PurpleBuddyPresence *		Purple__BuddyPresence;

/* proxy.h */
typedef PurpleProxyInfo *			Purple__ProxyInfo;
typedef PurpleProxyType			Purple__ProxyType;

/* prpl.h */
typedef PurpleBuddyIconSpec *		Purple__Buddy__Icon__Spec;
typedef PurpleIconScaleRules		Purple__IconScaleRules;
typedef PurplePluginProtocolInfo *	Purple__PluginProtocolInfo;
typedef PurpleProtocolOptions		Purple__ProtocolOptions;

/* request.h */
typedef PurpleRequestField *		Purple__Request__Field;
typedef PurpleRequestFields *		Purple__Request__Fields;
typedef PurpleRequestFieldGroup *		Purple__Request__Field__Group;
typedef PurpleRequestFieldType		Purple__RequestFieldType;
typedef PurpleRequestType			Purple__RequestType;

/* roomlist.h */
typedef PurpleRoomlist *			Purple__Roomlist;
typedef PurpleRoomlistField *		Purple__Roomlist__Field;
typedef PurpleRoomlistFieldType		Purple__RoomlistFieldType;
typedef PurpleRoomlistRoom *		Purple__Roomlist__Room;
typedef PurpleRoomlistRoomType		Purple__RoomlistRoomType;

/* savedstatuses.h */
typedef PurpleSavedStatus *		Purple__SavedStatus;
typedef PurpleSavedStatusSub *		Purple__SavedStatus__Sub;

/* smiley.h */
typedef PurpleSmiley *		Purple__Smiley;

/* sound.h */
typedef PurpleSoundEventID		Purple__SoundEventID;

/* sslconn.h */
typedef PurpleInputCondition *		Purple__Input__Condition;
typedef PurpleSslConnection *		Purple__Ssl__Connection;
typedef PurpleSslErrorType		Purple__SslErrorType;
typedef PurpleSslOps *			Purple__Ssl__Ops;

/* status.h */
typedef PurpleStatus *			Purple__Status;
typedef PurpleStatusAttr *		Purple__StatusAttr;
typedef PurpleStatusPrimitive		Purple__StatusPrimitive;
typedef PurpleStatusType *		Purple__StatusType;

/* stringref.h */
typedef PurpleStringref *			Purple__Stringref;

/* util.h */
typedef PurpleInfoFieldFormatCallback	Purple__Util__InfoFieldFormatCallback;
typedef PurpleMenuAction *		Purple__Menu__Action;

/* whiteboard.h */
typedef PurpleWhiteboard *		Purple__Whiteboard;

/* xmlnode.h */
typedef xmlnode *			Purple__XMLNode;
typedef XMLNodeType			XMLNode__Type;

/* other */
typedef void *				Purple__Handle;

typedef gchar gchar_own;

typedef struct _constiv {
	const char *name;
	IV iv;
} constiv;

