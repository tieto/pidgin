/**
 * @file gtkstock.h GTK+ Stock resources
 * @ingroup gtkui
 *
 * gaim
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#ifndef _PIDGIN_STOCK_H_
#define _PIDGIN_STOCK_H_

/**************************************************************************/
/** @name Stock images                                                    */
/**************************************************************************/
/*@{*/
#define PIDGIN_STOCK_ABOUT           "pidgin-about"
#define PIDGIN_STOCK_ACTION          "pidgin-action"
#define PIDGIN_STOCK_ALIAS           "pidgin-alias"
#define PIDGIN_STOCK_AWAY            "pidgin-away"
#define PIDGIN_STOCK_BLOCK           "pidgin-block"
#define PIDGIN_STOCK_UNBLOCK         "pidgin-unblock"
#define PIDGIN_STOCK_CHAT            "pidgin-chat"
#define PIDGIN_STOCK_CLEAR           "pidgin-clear"
#define PIDGIN_STOCK_CLOSE_TABS      "pidgin-close-tab"
#define PIDGIN_STOCK_CONNECT         "pidgin-connect"
#define PIDGIN_STOCK_DEBUG           "pidgin-debug"
#define PIDGIN_STOCK_DISCONNECT      "pidgin-disconnect"
#define PIDGIN_STOCK_DOWNLOAD        "pidgin-download"
#define PIDGIN_STOCK_EDIT            "pidgin-edit"
#define PIDGIN_STOCK_FGCOLOR         "pidgin-fgcolor"
#define PIDGIN_STOCK_FILE_CANCELED   "pidgin-file-canceled"
#define PIDGIN_STOCK_FILE_DONE       "pidgin-file-done"
#define PIDGIN_STOCK_FILE_TRANSFER   "pidgin-file-transfer"
#define PIDGIN_STOCK_ICON_AWAY       "pidgin-icon-away"
#define PIDGIN_STOCK_ICON_AWAY_MSG   "pidgin-icon-away-msg"
#define PIDGIN_STOCK_ICON_CONNECT    "pidgin-icon-away-connect"
#define PIDGIN_STOCK_ICON_OFFLINE    "pidgin-icon-offline"
#define PIDGIN_STOCK_ICON_ONLINE     "pidgin-icon-online"
#define PIDGIN_STOCK_ICON_ONLINE_MSG "pidgin-icon-online-msg"
#define PIDGIN_STOCK_IGNORE          "pidgin-ignore"
#define PIDGIN_STOCK_INVITE          "pidgin-invite"
#define PIDGIN_STOCK_MODIFY          "pidgin-modify"
#define PIDGIN_STOCK_OPEN_MAIL       "pidgin-stock-open-mail"
#define PIDGIN_STOCK_PAUSE           "pidgin-pause"
#define PIDGIN_STOCK_POUNCE          "pidgin-pounce"
#define PIDGIN_STOCK_SIGN_OFF        "pidgin-sign-off"
#define PIDGIN_STOCK_SIGN_ON         "pidgin-sign-on"
#define PIDGIN_STOCK_TEXT_NORMAL     "pidgin-text-normal"
#define PIDGIN_STOCK_TYPED           "pidgin-typed"
#define PIDGIN_STOCK_UPLOAD          "pidgin-upload"

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
#define PIDGIN_STOCK_STATUS_VOICE      "pidgin-status-voice"
#define PIDGIN_STOCK_STATUS_MESSAGE    "pidgin-status-message"

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
#define PIDGIN_STOCK_ANIMATION_TYPING4	"pidgin-anim-typing4"

/* Toolbar (and menu) icons */
#define PIDGIN_STOCK_TOOLBAR_ACCOUNTS     "pidgin-accounts"
#define PIDGIN_STOCK_TOOLBAR_BGCOLOR      "pidgin-bgcolor"
#define PIDGIN_STOCK_TOOLBAR_FGCOLOR      "pidgin-fgcolor"
#define PIDGIN_STOCK_TOOLBAR_SMILEY       "pidgin-smiley"
#define PIDGIN_STOCK_TOOLBAR_TEXT_SMALLER "pidgin-text-smaller"
#define PIDGIN_STOCK_TOOLBAR_TEXT_LARGER  "pidgin-text-larger"
#define PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE "pidgin-insert-image"
#define PIDGIN_STOCK_TOOLBAR_INSERT_LINK  "pidgin-insert-link"
#define PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW  "pidgin-message-new"
#define PIDGIN_STOCK_TOOLBAR_PLUGINS      "pidgin-plugins"
#define PIDGIN_STOCK_TOOLBAR_TYPING       "pidgin-typing"
#define PIDGIN_STOCK_TOOLBAR_USER_INFO    "pidgin-info"
#define PIDGIN_STOCK_TOOLBAR_PENDING      "pidgin-pending"

/* Tray icons */
#define PIDGIN_STOCK_TRAY_AVAILABLE       "pidgin-tray-available"
#define PIDGIN_STOCK_TRAY_AWAY	  	  "pidgin-tray-away"
#define PIDGIN_STOCK_TRAY_BUSY            "pidgin-tray-busy"
#define PIDGIN_STOCK_TRAY_XA              "pidgin-tray-xa"
#define PIDGIN_STOCK_TRAY_OFFLINE         "pidgin-tray-offline"
#define PIDGIN_STOCK_TRAY_CONNECT         "pidgin-tray-connect"
#define PIDGIN_STOCK_TRAY_PENDING         "pidgin-tray-pending"


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
void pidgin_stock_init(void);

#endif /* _PIDGIN_STOCK_H_ */
