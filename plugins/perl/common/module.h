

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
#include "util.h"
#include "value.h"
#include "xmlnode.h"

	/* account.h */
typedef GaimAccount *     	Gaim__Account;
typedef GaimAccountOption *	Gaim__Account__Option;    
typedef GaimAccountUiOps * 	Gaim__Account__UiOps;
typedef GaimAccountUserSplit *	Gaim__Account__UserSplit;	

	/* blist.h */
typedef GaimBlistNode *				Gaim__BuddyList__Node;  
typedef GaimBlistNodeAction *			Gaim__BuddyList__Node__Action;
typedef GaimBlistUiOps *			Gaim__BuddyList__UiOps;
typedef GaimBlistNodeFlags		Gaim__BlistNodeFlags;
typedef GaimBuddyList *		Gaim__BuddyList;
typedef GaimBuddy *       	Gaim__BuddyList__Buddy;
typedef GaimChat *   		Gaim__BuddyList__Chat;
typedef GaimContact *     	Gaim__BuddyList__Contact;
typedef GaimGroup *       	Gaim__BuddyList__Group;

	/* buddyicon.h */
typedef GaimBuddyIcon *				Gaim__Buddy__Icon;

	/* cipher.h */
typedef GaimCipherCaps			Gaim__CipherCaps;
typedef GaimCipher *			Gaim__Cipher;
typedef GaimCipherOps *			Gaim__Cipher__Ops;
typedef GaimCipherContext *		Gaim__Cipher__Context;	

	/* cmds.h */
typedef GaimCmdId              Gaim__CmdId;

	/* connection.h */
typedef GaimConnectionFlags		Gaim__ConnectionFlags;
typedef GaimConnectionState		Gaim__ConnectionState;
typedef GaimConnection *  			Gaim__Connection;
typedef GaimConnectionUiOps *			Gaim__Connection__UiOps;

	/* converstaion.h */
typedef GaimConversationType	  	Gaim__ConversationType;
typedef GaimUnseenState	  		Gaim__UnseenState;
typedef GaimConvUpdateType		Gaim__ConvUpdateType;
typedef GaimTypingState			Gaim__TypingState;
typedef GaimMessageFlags	  	Gaim__MessageFlags;
typedef GaimConvChatBuddyFlags	  	Gaim__ConvChatBuddyFlags;
typedef GaimConvWindowUiOps *			Gaim__ConvWindow__UiOps;
typedef GaimConvWindow *      			Gaim__ConvWindow;
typedef GaimConversationUiOps *			Gaim__Conversation__UiOps;
typedef GaimConversation *			Gaim__Conversation;
typedef GaimConvIm *          			Gaim__Conversation__IM;
typedef GaimConvChat *        			Gaim__Conversation__Chat;
typedef GaimConvChatBuddy *       		Gaim__Conversation__ChatBuddy;

	/* debug.h */
typedef GaimDebugLevel	  		Gaim__DebugLevel;

	/* desktopitem.h */
typedef GaimDesktopItem *               Gaim__DesktopItem;
typedef GaimDesktopItemType             Gaim__DesktopItemType;

	/* eventloop.h */
typedef GaimInputCondition *            Gaim__InputCondition;
typedef GaimEventLoopUiOps *            Gaim__EventLoopUiOps;

	/* ft.h */
typedef GaimXfer *              	Gaim__Xfer;
typedef GaimXferType            	Gaim__XferType;
typedef GaimXferStatusType              Gaim__XferStatusType;
typedef GaimXferUiOps	*		Gaim__XferUiOps;

	/* gtkblish.h */
typedef GaimGtkBuddyList *		Gaim__GTK__BuddyList;
typedef GaimStatusIconSize              Gaim__StatusIconSize;


	/* gtkutils.h */
typedef GaimButtonOrientation		Gaim__ButtonOrientation;
typedef GaimButtonStyle			Gaim__ButtonStyle;
#ifndef _WIN32
typedef GaimBrowserPlace		Gaim__BrowserPlace;
#endif /* _WIN32 */

	/* gtkconv.h */
typedef GdkPixbuf *			Gaim__GDK__Pixbuf;
typedef GtkWidget *			Gaim__GTK__Widget;
typedef GaimGtkConversation *		Gaim__GTK__Conversation;

	/* gtkutils.h */
typedef GtkSelectionData * 		Gaim__GTK__SelectionData;
typedef GtkFileSelection *		Gaim__GTK__FileSelection;
typedef GtkTextView *			Gaim__GTK__TextView;

	/* gtkconn.h */

	/* imgstore.h */
typedef GaimStoredImage *               Gaim__StoredImage;

	/* log.h */
typedef GaimLog * 			Gaim__Log;
typedef GaimLogLogger *         	Gaim__Log__Logger;
typedef GaimLogCommonLoggerData *       Gaim__LogCommonLoggerData;
typedef GaimLogSet *           	 	Gaim__LogSet;
typedef GaimLogType             	Gaim__LogType;
typedef GaimLogReadFlags *              Gaim__Log__ReadFlags;

	/* notify.h */
typedef GaimNotifyType          	Gaim__NotifyType;
typedef GaimNotifyMsgType               Gaim__NotifyMsgType;	
typedef GaimNotifyUiOps *		Gaim__NotifyUiOps;

	/* plugin.h */
typedef GaimPlugin *      		Gaim__Plugin;
typedef GaimPluginType			Gaim__PluginType;
typedef GaimPluginInfo *                Gaim__PluginInfo;
typedef GaimPluginUiInfo *              Gaim__PluginUiInfo;
typedef GaimPluginLoaderInfo *          Gaim__PluginLoaderInfo;
typedef GaimPluginAction *              Gaim__Plugin__Action;

	/* pluginpref.h */
typedef GaimPluginPrefFrame *           Gaim__PluginPrefFrame;
typedef GaimPluginPref *                Gaim__PluginPref;
typedef GaimPluginPrefType              Gaim__PluginPrefType;

	/* pounce.h */
typedef GaimPounce *            Gaim__Pounce;
typedef GaimPounceEvent         Gaim__PounceEvent;


	/* prefs.h */
typedef GaimPrefType           	 Gaim__PrefType;

	/* privacy.h */
typedef GaimPrivacyType         Gaim__PrivacyType;
typedef GaimPrivacyUiOps *	Gaim__Privacy__UiOps; 

	/* proxy.h */
typedef GaimProxyType           Gaim__ProxyType;
typedef GaimProxyInfo *		Gaim__ProxyInfo;


	/* prpl.h */
typedef GaimBuddyIconSpec *				Gaim__Buddy__Icon__Spec;
typedef GaimPluginProtocolInfo *		Gaim__PluginProtocolInfo;
typedef GaimConvImFlags         		Gaim__ConvImFlags;
typedef GaimConvChatFlags               Gaim__ConvChatFlags;
typedef GaimIconScaleRules              Gaim__IconScaleRules;
typedef GaimProtocolOptions             Gaim__ProtocolOptions;

	/* request.h */
typedef GaimRequestType         Gaim__RequestType;
typedef GaimRequestFieldType            Gaim__RequestFieldType;
typedef GaimRequestFields *		Gaim__Request__Fields;
typedef GaimRequestFieldGroup *		Gaim__Request__Field__Group;
typedef GaimRequestField *		Gaim__Request__Field;
typedef GaimRequestUiOps *		Gaim__Request__UiOps;

	/* roomlist.h */
	
typedef GaimRoomlist *          Gaim__Roomlist;
typedef GaimRoomlistRoom *              Gaim__Roomlist__Room;
typedef GaimRoomlistField *             Gaim__Roomlist__Field;
typedef GaimRoomlistUiOps *             Gaim__Roomlist__UiOps;
typedef GaimRoomlistRoomType            Gaim__RoomlistRoomType;
typedef GaimRoomlistFieldType           Gaim__RoomlistFieldType;

	/* savedstatuses.h */
typedef GaimSavedStatus *               Gaim__SavedStatus;
typedef GaimSavedStatusSub *            Gaim__SavedStatusSub;

	/* sound.h */
typedef GaimSoundEventID                Gaim__SoundEventID;
typedef GaimSoundUiOps 	*		Gaim__Sound__UiOps;

	/* sslconn.h */
typedef GaimSslConnection *             Gaim__Ssl__Connection;
typedef GaimInputCondition *            Gaim__Input__Condition;
typedef GaimSslErrorType                Gaim__SslErrorType;
typedef GaimSslOps *			Gaim__Ssl__Ops;

	/* status.h */
typedef GaimStatusType *                Gaim__StatusType;
typedef GaimStatusAttr *                Gaim__StatusAttr;
typedef GaimPresence *          	Gaim__Presence;
typedef GaimStatus *            	Gaim__Status;
typedef GaimPresenceContext             Gaim__PresenceContext;
typedef GaimStatusPrimitive             Gaim__StatusPrimitive;

typedef GaimStringref *			Gaim__Stringref;

	/* value.h */
typedef GaimValue *			Gaim__Value;

	/* xmlnode.h */
typedef XMLNodeType             XMLNode__Type;


	/* other.h */

