/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
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
 *
 */

#ifdef USE_OSCAR

#include <netdb.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "gaim.h"
#include <aim.h>
#include "gnome_applet_mgr.h"

struct aim_conn_t *gaim_conn = NULL;
static int inpa = -1;

int gaim_auth_failure(struct command_rx_struct *command, ...);
int gaim_auth_success(struct command_rx_struct *command, ...);
int gaim_serverready_handle(struct command_rx_struct *command, ...);
int gaim_redirect_handle(struct command_rx_struct *command, ...);
int gaim_im_handle(struct command_rx_struct *command, ...);

rxcallback_t gaim_callbacks[] = {
        gaim_im_handle,                 /* incoming IM */
        NULL,/*gaim_buddy_coming,               oncoming buddy */
        NULL,/*gaim_buddy_going,                offgoing buddy */
        NULL,                           /* last IM was missed 1 */
        NULL,                           /* last IM was missed 2 */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        gaim_serverready_handle,        /* server ready */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        NULL,                   /* UNUSED */
        gaim_redirect_handle,           /* redirect */
        NULL,                           /* last command bad */
        NULL,                           /* missed some messages */
        NULL,                           /* completely unknown command */
        NULL, /*gaim_userinfo_handler,           User Info Response */
        NULL,                           /* Address search response */
        NULL,                           /* Name search response */
        NULL,                           /* User Search fail */
        gaim_auth_failure,              /* auth error */
        gaim_auth_success,              /* auth success */
        NULL,                           /* auth server ready */
        NULL,                           /* ? */
        NULL,                           /* password change done */
        gaim_serverready_handle,        /* server ready */
        0x00
};

struct client_info_s cinfo;


void oscar_close()
{
#ifdef USE_APPLET
	setUserState(offline);
#endif /* USE_APPLET */
        set_state(STATE_OFFLINE);
        aim_conn_close(gaim_conn);
        if (inpa > 0)
                gdk_input_remove(inpa);
	inpa=-1;
}


void oscar_callback(gpointer data, gint source, GdkInputCondition condition)
{
        if (aim_get_command() < 0) {
                signoff();
                hide_login_progress("Connection Closed");
                return;
        } else
                aim_rxdispatch();

}

int oscar_login(char *username, char *password)
{
        char buf[256];
        struct timeval timeout;
        time_t lastcycle=0;
        
        aim_connrst();
        aim_register_callbacks(gaim_callbacks);

        aim_conn_getnext()->fd = STDIN_FILENO;

	spintf(buf, "Looking up %s", login_host);
        set_login_progress(1, buf);

        gaim_conn = aim_newconn(AIM_CONN_TYPE_AUTH, login_host);

        if (!gaim_conn) {
#ifdef USE_APPLET
                setUserState(offline);
#endif /* USE_APPLET */
                set_state(STATE_OFFLINE);
                hide_login_progress("Unable to login to AIM");
                return -1;
        } else if (gaim_conn->fd == -1) {
#ifdef USE_APPLET
                setUserState(offline);
#endif /* USE_APPLET */
                set_state(STATE_OFFLINE);
                
                if (gaim_conn->status & AIM_CONN_STATUS_RESOLVERR) {
			sprintf(buf, "Unable to lookup %s", login_host);
                        hide_login_progress(buf);
                } else if (gaim_conn->status & AIM_CONN_STATUS_CONNERR) {
			sprintf(buf, "Unable to connect to %s", login_host);
                        hide_login_progress(buf);
                }
                return -1;
        }

        g_snprintf(buf, sizeof(buf), "Signon: %s",username);
	
        set_login_progress(2, buf);

        strcpy(cinfo.clientstring, "libfaim/GAIM, jimduchek@ou.edu, see at http://www.marko.net/gaim");
        cinfo.major = 0;
        cinfo.minor = 9;
        cinfo.build = 7;
        strcpy(cinfo.country, "us");
        strcpy(cinfo.lang, "en");

        aim_send_login(gaim_conn, username, password, &cinfo);

        if (!current_user) {
                current_user = g_new0(struct aim_user, 1);
                g_snprintf(current_user->username, sizeof(current_user->username), DEFAULT_INFO);
                aim_users = g_list_append(aim_users, current_user);
        }

        g_snprintf(current_user->username, sizeof(current_user->username), "%s", username);
        g_snprintf(current_user->password, sizeof(current_user->password), "%s", password);

        save_prefs();

        inpa = gdk_input_add(gaim_conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, oscar_callback, NULL);

        return 0;
}

int gaim_auth_success(struct command_rx_struct *command, ...)
{
        va_list ap;
        struct login_phase1_struct *logininfo;
        struct aim_conn_t *bosconn = NULL;
        char buf[128];

        va_start(ap, command);
        logininfo = va_arg(ap, struct login_phase1_struct *);
        va_end(ap);

        g_snprintf(buf, sizeof(buf), "Auth successful, logging in to %s:", logininfo->BOSIP);
        set_login_progress(3, buf);
        
        printf("          Screen name: %s\n", logininfo->screen_name);
        printf("       Email addresss: %s\n", logininfo->email);
        printf("  Registration status: %02i\n", logininfo->regstatus);
        printf("Connecting to %s, closing auth connection.\n",
                logininfo->BOSIP);

        aim_conn_close(command->conn);

        gdk_input_remove(inpa);

        if ((bosconn = aim_newconn(AIM_CONN_TYPE_BOS, logininfo->BOSIP))
            == NULL) {
#ifdef USE_APPLET
                setUserState(offline);
#endif /* USE_APPLET */
                set_state(STATE_OFFLINE);

                hide_login_progress("Could not connect to BOS: internal error");
                return(-1);
        } else if (bosconn->status != 0) {
#ifdef USE_APPLET
                setUserState(offline);
#endif /* USE_APPLET */
                set_state(STATE_OFFLINE);

                hide_login_progress("Could not connect to BOS");
                return(-1);
        } else {
                aim_auth_sendcookie(bosconn, logininfo->cookie);
                inpa = gdk_input_add(bosconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, oscar_callback, NULL);
                set_login_progress(4, "BOS connection established, cookie sent.");
                return(1);
        }
}

int gaim_auth_failure(struct command_rx_struct *command, ...)
{
        va_list ap;
        struct login_phase1_struct      *logininfo;
        char    *errorurl;
        short   errorcode;

        va_start(ap, command);
        logininfo = va_arg(ap, struct login_phase1_struct *);
        printf("Screen name: %s\n", logininfo->screen_name);
        errorurl = va_arg(ap, char *);
        printf("Error URL: %s\n", errorurl);
        errorcode = va_arg(ap, short);
        printf("Error code: 0x%02x\n", errorcode);
        va_end(ap);
#ifdef USE_APPLET
        setUserState(offline);
#endif /* USE_APPLET */
        set_state(STATE_OFFLINE);
        hide_login_progress("Authentication Failed");

        aim_conn_close(aim_getconn_type(AIM_CONN_TYPE_AUTH));

        return 1;
}

int gaim_serverready_handle(struct command_rx_struct *command, ...)
{
        switch (command->conn->type) {
        case AIM_CONN_TYPE_BOS:
                aim_bos_reqrate(command->conn);
                aim_bos_ackrateresp(command->conn);
                aim_bos_setprivacyflags(command->conn, 0x00000003);
                aim_bos_reqservice(command->conn, AIM_CONN_TYPE_ADS);
                aim_bos_setgroupperm(NULL, 0x1f);
                break;
        case AIM_CONN_TYPE_CHATNAV:
		break;
        default:
		printf("Unknown connection type on serverready\n");
		break;
        }
        return(1);

}

int gaim_redirect_handle(struct command_rx_struct *command, ...)
{
        va_list ap;
        int     serviceid;
        char    *ip, *cookie;

        va_start(ap, command);
        serviceid = va_arg(ap, int);
        ip = va_arg(ap, char *);
        cookie = va_arg(ap, char *);
        va_end(ap);

        switch(serviceid) {
        case 0x0005: {
                char *buf;
                char *buf2;
                char file[1024];
                FILE *f;

                g_snprintf(file, sizeof(file), "%s/.gaimbuddy", getenv("HOME"));
        
                if (!(f = fopen(file,"r"))) {
                } else {
                        buf = g_malloc(BUF_LONG);
                        fread(buf, BUF_LONG, 1, f);

                        parse_toc_buddy_list(buf);

                        build_edit_tree();
                        build_permit_tree();
        

                        g_free(buf);
                }
                


                aim_bos_clientready(command->conn);

		set_login_progress(5, "Logged in.\n");
#ifdef USE_APPLET
		if (general_options & OPT_GEN_APP_BUDDY_SHOW) {
			show_buddy_list();
			refresh_buddy_window();
		} else {
		}

		set_applet_draw_closed();
		setUserState(online);
#else
		gtk_widget_hide(mainwindow);
		show_buddy_list();
		refresh_buddy_window();
#endif
		serv_finish_login();
		gaim_conn = command->conn;

                break;
        }
	case 0x0007: {
		struct aim_conn_t       *tstconn;

		tstconn = aim_newconn(AIM_CONN_TYPE_AUTH, ip);
		if ((tstconn == NULL) ||
		    (tstconn->status >= AIM_CONN_STATUS_RESOLVERR)) {
#ifdef USE_APPLET
			setUserState(offline);
#endif /* USE_APPLET */
			set_state(STATE_OFFLINE);
			hide_login_progress("Unable to reconnect to authorizer");
		} else
			aim_auth_sendcookie(tstconn, cookie);
		break;
	}
	case 0x000d: {
		struct aim_conn_t       *tstconn;

		tstconn = aim_newconn(AIM_CONN_TYPE_CHATNAV, ip);
		if ((tstconn == NULL) ||
		    (tstconn->status >= AIM_CONN_STATUS_RESOLVERR))
			printf("Unable to connect to chatnav server\n");
		else
			aim_auth_sendcookie(
					    aim_getconn_type(AIM_CONN_TYPE_CHATNAV),
					    cookie);
		break;
	}
	case 0x000e:
		printf("CHAT is not yet supported :(\n");
		break;
	default:
		printf("Unknown redirect %#04X\n", serviceid);
		break;
	}
	return(1);
		
}



int gaim_im_handle(struct command_rx_struct *command, ...)
{
        time_t  t = 0;
        char    *screenname, *msg;
        int     warninglevel, class, idletime, isautoreply;
        ulong   membersince, onsince;
        va_list ap;

        va_start(ap, command);
	screenname = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	warninglevel = va_arg(ap, int);
	class = va_arg(ap, int);
        membersince = va_arg(ap, ulong);
        onsince = va_arg(ap, ulong);
        idletime = va_arg(ap, int);
        isautoreply = va_arg(ap, int);
        va_end(ap);

        printf("'%s'\n", msg);
        
        serv_got_im(screenname, msg, isautoreply);

        return(1);


}

#endif
