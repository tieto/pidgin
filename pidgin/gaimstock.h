/**
 * @file gtkstock.h GTK+ Stock resources
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <gtk/gtkstock.h>

#ifndef _GAIM_STOCK_H_
#define _GAIM_STOCK_H_

/**************************************************************************/
/** @name Stock images                                                    */
/**************************************************************************/
/*@{*/
#define GAIM_STOCK_ABOUT           "gaim-about"
#define GAIM_STOCK_ACCOUNTS        "gaim-accounts"
#define GAIM_STOCK_ACTION          "gaim-action"
#define GAIM_STOCK_ALIAS           "gaim-alias"
#define GAIM_STOCK_AWAY            "gaim-away"
#define GAIM_STOCK_BGCOLOR         "gaim-bgcolor"
#define GAIM_STOCK_BLOCK           "gaim-block"
#define GAIM_STOCK_UNBLOCK         "gaim-unblock"
#define GAIM_STOCK_CHAT            "gaim-chat"
#define GAIM_STOCK_CLEAR           "gaim-clear"
#define GAIM_STOCK_CLOSE_TABS      "gaim-close-tab"
#define GAIM_STOCK_CONNECT         "gaim-connect"
#define GAIM_STOCK_DEBUG           "gaim-debug"
#define GAIM_STOCK_DISCONNECT      "gaim-disconnect"
#define GAIM_STOCK_DOWNLOAD        "gaim-download"
#define GAIM_STOCK_EDIT            "gaim-edit"
#define GAIM_STOCK_FGCOLOR         "gaim-fgcolor"
#define GAIM_STOCK_FILE_CANCELED   "gaim-file-canceled"
#define GAIM_STOCK_FILE_DONE       "gaim-file-done"
#define GAIM_STOCK_FILE_TRANSFER   "gaim-file-transfer"
#define GAIM_STOCK_ICON_AWAY       "gaim-icon-away"
#define GAIM_STOCK_ICON_AWAY_MSG   "gaim-icon-away-msg"
#define GAIM_STOCK_ICON_CONNECT    "gaim-icon-away-connect"
#define GAIM_STOCK_ICON_OFFLINE    "gaim-icon-offline"
#define GAIM_STOCK_ICON_ONLINE     "gaim-icon-online"
#define GAIM_STOCK_ICON_ONLINE_MSG "gaim-icon-online-msg"
#define GAIM_STOCK_IGNORE          "gaim-ignore"
#define GAIM_STOCK_IM              "gaim-im"
#define GAIM_STOCK_IMAGE           "gaim-image"
#define GAIM_STOCK_INFO            "gaim-info"
#define GAIM_STOCK_INVITE          "gaim-invite"
#define GAIM_STOCK_LINK            "gaim-link"
#define GAIM_STOCK_LOG             "gaim-log"
#define GAIM_STOCK_MODIFY          "gaim-modify"
#define GAIM_STOCK_OPEN_MAIL       "gaim-stock-open-mail"
#define GAIM_STOCK_PAUSE           "gaim-pause"
#define GAIM_STOCK_PENDING         "gaim-pending"
#define GAIM_STOCK_PLUGIN          "gaim-plugin"
#define GAIM_STOCK_POUNCE          "gaim-pounce"
#define GAIM_STOCK_SEND            "gaim-send"
#define GAIM_STOCK_SIGN_OFF        "gaim-sign-off"
#define GAIM_STOCK_SIGN_ON         "gaim-sign-on"
#define GAIM_STOCK_SMILEY          "gaim-smiley"
#define GAIM_STOCK_STATUS_ONLINE   "gaim-status-online"
#define GAIM_STOCK_STATUS_AWAY     "gaim-status-away"
#define GAIM_STOCK_STATUS_INVISIBLE "gaim-status-invisible"
#define GAIM_STOCK_STATUS_OFFLINE   "gaim-status-offline"
#define GAIM_STOCK_STATUS_TYPING0  "gaim-status-typing0"
#define GAIM_STOCK_STATUS_TYPING1  "gaim-status-typing1"
#define GAIM_STOCK_STATUS_TYPING2  "gaim-status-typing2"
#define GAIM_STOCK_STATUS_TYPING3  "gaim-status-typing3"
#define GAIM_STOCK_STATUS_CONNECT0 "gaim-status-connect0"
#define GAIM_STOCK_STATUS_CONNECT1 "gaim-status-connect1"
#define GAIM_STOCK_STATUS_CONNECT2 "gaim-status-connect2"
#define GAIM_STOCK_STATUS_CONNECT3 "gaim-status-connect3"
#define GAIM_STOCK_TEXT_BIGGER     "gaim-text-bigger"
#define GAIM_STOCK_TEXT_NORMAL     "gaim-text-normal"
#define GAIM_STOCK_TEXT_SMALLER    "gaim-text-smaller"
#define GAIM_STOCK_TYPED           "gaim-typed"
#define GAIM_STOCK_TYPING          "gaim-typing"
#define GAIM_STOCK_UPLOAD          "gaim-upload"
#define GAIM_STOCK_VOICE_CHAT      "gaim-voice-chat"

/* Status icons */
#define PIDGIN_STOCK_STATUS_AVAILABLE  "pidgin-status-available"
#define PIDGIN_STOCK_STATUS_AVAILABLE_I "pidgin-status-available-i"
#define PIDGIN_STOCK_STATUS_AWAY       "pidgin-status-away"
#define PIDGIN_STOCK_STATUS_AWAY_I     "pidgin-status-away-i"
#define PIDGIN_STOCK_STATUS_BUSY       "pidgin-status-busy"
#define PIDGIN_STOCK_STATUS_BUSY_I     "pidgin-status-busy-i"
#define PIDGIN_STOCK_STATUS_CHAT       "pidgin-status-chat"
#define PIDGIN_STOCK_STATUS_XA         "pidgin-status-xa"
#define PIDGIN_STOCK_STATUS_XA_I       "pidgin-status-xa-i"
#define PIDGIN_STOCK_STATUS_LOGIN      "pidgin-status-login"
#define PIDGIN_STOCK_STATUS_LOGOUT     "pidgin-status-logout"
#define PIDGIN_STOCK_STATUS_OFFLINE    "pidgin-status-offline"
#define PIDGIN_STOCK_STATUS_PERSON     "pidgin-status-person"
#define PIDGIN_STOCK_STATUS_OPERATOR   "pidgin-status-operator"
#define PIDGIN_STOCK_STATUS_HALFOP     "pidgin-status-halfop"

/* Dialog icons */
#define PIDGIN_STOCK_DIALOG_AUTH	"pidgin-dialog-auth"
#define PIDGIN_STOCK_DIALOG_ERROR	"pidgin-dialog-error"
#define PIDGIN_STOCK_DIALOG_INFO	"pidgin-dialog-info"
#define PIDGIN_STOCK_DIALOG_MAIL	"pidgin-dialog-mail"
#define PIDGIN_STOCK_DIALOG_QUESTION	"pidgin-dialog-question"
#define PIDGIN_STOCK_DIALOG_COOL	"pidgin-dialog-cool"
#define PIDGIN_STOCK_DIALOG_WARNING	"pidgin-dialog-warning"

/* StatusBox Animations */
#define PIDGIN_STOCK_ANIMATION_CONNECT0 "pidgin-anim-connect0"
#define PIDGIN_STOCK_ANIMATION_CONNECT1 "pidgin-anim-connect1"
#define PIDGIN_STOCK_ANIMATION_CONNECT2 "pidgin-anim-connect2"
#define PIDGIN_STOCK_ANIMATION_CONNECT3 "pidgin-anim-connect3"
#define PIDGIN_STOCK_ANIMATION_TYPING0	"pidgin-anim-typing0"
#define PIDGIN_STOCK_ANIMATION_TYPING1	"pidgin-anim-typing1"
#define PIDGIN_STOCK_ANIMATION_TYPING2	"pidgin-anim-typing2"
#define PIDGIN_STOCK_ANIMATION_TYPING3	"pidgin-anim-typing3"

/*@}*/

/**
 * For using icons that aren't one of the default GTK_ICON_SIZEs
 */
#define PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL    "pidgin-icon-size-tango-extra-small"
#define PIDGIN_ICON_SIZE_TANGO_SMALL          "pidgin-icon-size-tango-small"
#define PIDGIN_ICON_SIZE_TANGO_MEDIUM         "pidgin-icon-size-tango-medium"
#define PIDGIN_ICON_SIZE_TANGO_HUGE           "pidgin-icon-size-tango-huge"
/**
 * Sets up the gaim stock repository.
 */
void gaim_gtk_stock_init(void);

#endif /* _GAIM_STOCK_H_ */
