/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * gaim
 *
 * Copyright (C) 1998-2001, Mark Spencer <markster@marko.net>
 * Some code borrowed from GtkZephyr, by
 * 	Jag/Sean Dilda <agrajag@linuxpower.org>/<smdilda@unity.ncsu.edu>
 * 	http://gtkzephyr.linuxpower.org/
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <string.h>
#include "gaim.h"
#include "prpl.h"
#include "zephyr/zephyr.h"

char *name()
{
	return "Zephyr";
}

char *description()
{
	return "Allows gaim to use the Zephyr protocol";
}

static char *zephyr_name()
{
	return "Zephyr";
}

#define z_call(func)		if (func != ZERR_NONE)\
					return;
#define z_call_r(func)		if (func != ZERR_NONE)\
					return TRUE;
#define z_call_s(func, err)	if (func != ZERR_NONE) {\
					hide_login_progress(zgc, err);\
					signoff(zgc);\
					return;\
				}

/* this is so bad, and if Zephyr weren't so fucked up to begin with I
 * wouldn't do this. but it is so i will. */
static guint32 nottimer = 0;
static guint32 loctimer = 0;
struct gaim_connection *zgc = NULL;
static GList *pending_zloc_names = NULL;

/* just for debugging
static void handle_unknown(ZNotice_t notice)
{
	g_print("z_packet: %s\n", notice.z_packet);
	g_print("z_version: %s\n", notice.z_version);
	g_print("z_kind: %d\n", notice.z_kind);
	g_print("z_class: %s\n", notice.z_class);
	g_print("z_class_inst: %s\n", notice.z_class_inst);
	g_print("z_opcode: %s\n", notice.z_opcode);
	g_print("z_sender: %s\n", notice.z_sender);
	g_print("z_recipient: %s\n", notice.z_recipient);
	g_print("z_message: %s\n", notice.z_message);
	g_print("z_message_len: %d\n", notice.z_message_len);
	g_print("\n");
}
*/

static char *zephyr_normalize(const char *orig)
{
	static char buf[80];
	if (strchr(orig, '@')) {
		g_snprintf(buf, 80, "%s", orig);
	} else {
		g_snprintf(buf, 80, "%s@%s", orig, ZGetRealm());
	}
	return buf;
}

static gboolean pending_zloc(char *who)
{
	GList *curr;
	for (curr = pending_zloc_names; curr != NULL; curr = curr->next) {
		if (!g_strcasecmp(who, (char*)curr->data)) {
			g_free((char*)curr->data);
			pending_zloc_names = g_list_remove(pending_zloc_names, curr->data);
			return TRUE;
		}
	}
	return FALSE;
}

static void zephyr_zloc(struct gaim_connection *gc, char *who)
{
	ZAsyncLocateData_t ald;
	
	if (ZRequestLocations(zephyr_normalize(who), &ald, UNACKED, ZAUTH)
					!= ZERR_NONE) {
		return;
	}
	pending_zloc_names = g_list_append(pending_zloc_names,
					g_strdup(zephyr_normalize(who)));
}

static void info_callback(GtkObject *obj, char *who)
{
	zephyr_zloc(gtk_object_get_user_data(obj), who);
}

static void zephyr_buddy_menu(GtkWidget *menu, struct gaim_connection *gc, char *who)
{
	GtkWidget *button;

	button = gtk_menu_item_new_with_label(_("ZLocate"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(info_callback), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

static void handle_message(ZNotice_t notice, struct sockaddr_in from)
{
	if (!g_strcasecmp(notice.z_class, LOGIN_CLASS)) {
		/* well, we'll be updating in 2 seconds anyway, might as well ignore this. */
	} else if (!g_strcasecmp(notice.z_class, LOCATE_CLASS)) {
		if (!g_strcasecmp(notice.z_opcode, LOCATE_LOCATE)) {
			int nlocs;
			char *user;
			struct buddy *b;

			if (ZParseLocations(&notice, NULL, &nlocs, &user) != ZERR_NONE)
				return;
			if ((b = find_buddy(zgc, user)) == NULL) {
				char *e = strchr(user, '@');
				if (e) *e = '\0';
				b = find_buddy(zgc, user);
			}
			if (!b) {
				free(user);
				return;
			}
			if (pending_zloc(b->name)) {
				ZLocations_t locs;
				int one = 1;
				GString *str = g_string_new("");
				g_string_sprintfa(str, "<b>User:</b> %s<br>"
								"<b>Alias:</b> %s<br>",
								b->name, b->show);
				if (!nlocs) {
					g_string_sprintfa(str, "<br>Hidden or not logged-in");
				}
				for (; nlocs > 0; nlocs--) {
					ZGetLocations(&locs, &one);
					g_string_sprintfa(str, "<br>At %s since %s", locs.host,
									locs.time);
				}
				g_show_info_text(str->str);
				g_string_free(str, TRUE);
			}
			serv_got_update(zgc, b->name, nlocs, 0, 0, 0, 0, 0);

			free(user);
		}
	} else if (!g_strcasecmp(notice.z_class, "MESSAGE")) {
		char buf[BUF_LONG];
		char *ptr = notice.z_message + strlen(notice.z_message) + 1;
		int len = notice.z_message_len - (ptr - notice.z_message);
		if (len > 0) {
			g_snprintf(buf, len + 1, "%s", ptr);
			g_strchomp(buf);
			serv_got_im(zgc, notice.z_sender, buf, 0);
		}
	} else {
		/* yes. */
	}
}

static gint check_notify(gpointer data)
{
	while (ZPending()) {
		ZNotice_t notice;
		struct sockaddr_in from;
		z_call_r(ZReceiveNotice(&notice, &from));

		switch (notice.z_kind) {
		case UNSAFE:
		case UNACKED:
		case ACKED:
			handle_message(notice, from);
			break;
		default:
			/* we'll just ignore things for now */
			debug_printf("ZEPHYR: Unhandled Notice\n");
			break;
		}

		ZFreeNotice(&notice);
	}

	return TRUE;
}

static gint check_loc(gpointer data)
{
	GSList *gr, *m;
	ZAsyncLocateData_t ald;

	ald.user = NULL;
	memset(&(ald.uid), 0, sizeof(ZUnique_Id_t));
	ald.version = NULL;

	gr = zgc->groups;
	while (gr) {
		struct group *g = gr->data;
		m = g->members;
		while (m) {
			struct buddy *b = m->data;
			char *chk;
			chk = g_strdup(zephyr_normalize(b->name));
			/* doesn't matter if this fails or not; we'll just move on to the next one */
			ZRequestLocations(chk, &ald, UNACKED, ZAUTH);
			g_free(chk);
			m = m->next;
		}
		gr = gr->next;
	}

	return TRUE;
}

static char *get_exposure_level()
{
	char *exposure = ZGetVariable("exposure");

	if (!exposure)
		return EXPOSE_REALMVIS;
	if (!g_strcasecmp(exposure, EXPOSE_NONE))
		return EXPOSE_NONE;
	if (!g_strcasecmp(exposure, EXPOSE_OPSTAFF))
		return EXPOSE_OPSTAFF;
	if (!g_strcasecmp(exposure, EXPOSE_REALMANN))
		return EXPOSE_REALMANN;
	if (!g_strcasecmp(exposure, EXPOSE_NETVIS))
		return EXPOSE_NETVIS;
	if (!g_strcasecmp(exposure, EXPOSE_NETANN))
		return EXPOSE_NETANN;
	return EXPOSE_REALMVIS;
}

static void strip_comments(char *str)
{
	char *tmp = strchr(str, '#');
	if (tmp)
		*tmp = '\0';
	g_strchug(str);
	g_strchomp(str);
}

static void process_zsubs()
{
	FILE *f;
	gchar *fname;
	gchar buff[BUFSIZ];
	
	fname = g_strdup_printf("%s/.zephyr.subs", g_getenv("HOME"));
	f = fopen(fname, "r");
	if (f) {
		char **triple;
		ZSubscription_t sub;
		char *recip;
		while (fgets(buff, BUFSIZ, f)) {
			strip_comments(buff);
			if (buff[0]) {
				triple = g_strsplit(buff, ",", 3);
				if (triple[0] && triple[1] && triple[2]) {
					sub.zsub_class = triple[0];
					sub.zsub_classinst = triple[1];
					if (!g_strcasecmp(triple[2], "%me%")) {
						recip = g_strdup_printf("%s@%s", g_getenv("USER"),
										ZGetRealm());
					} else if (!g_strcasecmp(triple[2], "*")) {
						/* wildcard */
						recip = g_strdup_printf("@%s", ZGetRealm());
					} else {
						recip = g_strdup(triple[2]);
					}
					sub.zsub_recipient = recip;
					if (ZSubscribeTo(&sub, 1, 0) != ZERR_NONE) {
						debug_printf("Zephyr: Couldn't subscribe to %s, %s, "
									   "%s\n",
										sub.zsub_class,
										sub.zsub_classinst,
										sub.zsub_recipient);
					}
					g_free(recip);
				}
				g_strfreev(triple);
			}
		}
	}
}

static void process_anyone()
{
	FILE *fd;
	gchar buff[BUFSIZ], *filename;

	filename = g_strconcat(g_get_home_dir(), "/.anyone", NULL);
	if ((fd = fopen(filename, "r")) != NULL) {
		while (fgets(buff, BUFSIZ, fd)) {
			strip_comments(buff);
			if (buff[0])
				add_buddy(zgc, "Anyone", buff, buff);
		}
		fclose(fd);
	}
	g_free(filename);
}

static void zephyr_login(struct aim_user *user)
{
	ZSubscription_t sub;

	if (zgc) {
		do_error_dialog("Already logged in with Zephyr", "Zephyr");
		return;
	}

	zgc = new_gaim_conn(user);

	z_call_s(ZInitialize(), "Couldn't initialize zephyr");
	z_call_s(ZOpenPort(NULL), "Couldn't open port");
	z_call_s(ZSetLocation(get_exposure_level()), "Couldn't set location");

	sub.zsub_class = "MESSAGE";
	sub.zsub_classinst = "PERSONAL";
	sub.zsub_recipient = ZGetSender();

	/* we don't care if this fails. i'm lying right now. */
	if (ZSubscribeTo(&sub, 1, 0) != ZERR_NONE) {
		debug_printf("Zephyr: Couldn't subscribe to messages!\n");
	}

	account_online(zgc);
	serv_finish_login(zgc);

	if (bud_list_cache_exists(zgc))
		do_import(NULL, zgc);
	process_anyone();
	/* call process_zsubs to subscribe. still commented out since I don't know
	 * how you want to handle incoming msgs from subs.
	process_zsubs(); */

	nottimer = gtk_timeout_add(100, check_notify, NULL);
	loctimer = gtk_timeout_add(2000, check_loc, NULL);
}

static void zephyr_close(struct gaim_connection *gc)
{
	g_list_foreach(pending_zloc_names, (GFunc)g_free, NULL);
	g_list_free(pending_zloc_names);
	/* should probably write .anyone, but eh. we all use gaim exclusively, right? :-P */
	if (nottimer)
		gtk_timeout_remove(nottimer);
	nottimer = 0;
	if (loctimer)
		gtk_timeout_remove(loctimer);
	loctimer = 0;
	zgc = NULL;
	z_call(ZCancelSubscriptions(0));
	z_call(ZUnsetLocation());
	z_call(ZClosePort());
}

static void zephyr_add_buddy(struct gaim_connection *gc, char *buddy) { }
static void zephyr_remove_buddy(struct gaim_connection *gc, char *buddy) { }

static void zephyr_send_im(struct gaim_connection *gc, char *who, char *im, int away) {
	ZNotice_t notice;
	char *buf;
	char *sig;

	sig = ZGetVariable("zwrite-signature");
	if (!sig) {
		sig = g_get_real_name();
	}
	buf = g_strdup_printf("%s%c%s", sig, '\0', im);

	bzero((char *)&notice, sizeof(notice));
	notice.z_kind = ACKED;
	notice.z_port = 0;
	notice.z_opcode = "";
	notice.z_class = "MESSAGE";
	notice.z_class_inst = "PERSONAL";
	notice.z_sender = 0;
	notice.z_recipient = who;
	notice.z_default_format =
		"Class $class, Instance $instance:\n"
		"To: @bold($recipient) at $time $date\n"
		"From: @bold($1) <$sender>\n\n$2";
	notice.z_message_len = strlen(im) + strlen(sig) + 4;
	notice.z_message = buf;
	ZSendNotice(&notice, ZAUTH);
	g_free(buf);
}

static struct prpl *my_protocol = NULL;

void zephyr_init(struct prpl *ret)
{
	ret->protocol = PROTO_ZEPHYR;
	ret->name = zephyr_name;
	ret->login = zephyr_login;
	ret->close = zephyr_close;
	ret->add_buddy = zephyr_add_buddy;
	ret->remove_buddy = zephyr_remove_buddy;
	ret->send_im = zephyr_send_im;
	ret->get_info = zephyr_zloc;
	ret->normalize = zephyr_normalize;
	ret->buddy_menu = zephyr_buddy_menu;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule *handle)
{
	load_protocol(zephyr_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_ZEPHYR);
	if (p == my_protocol)
		unload_protocol(p);
}
