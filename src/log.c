/* ---------------------------------------------------
 * Function to remove a log file entry
 * ---------------------------------------------------
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "gaim.h"
#include "core.h"
#include "multi.h"
#include "prpl.h"
#include <sys/stat.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

void rm_log(struct log_conversation *a)
{
	struct conversation *cnv = find_conversation(a->name);

	log_conversations = g_list_remove(log_conversations, a);

	save_prefs();

	if (cnv && !(im_options & OPT_IM_ONE_WINDOW))
		set_convo_title(cnv);
}

struct log_conversation *find_log_info(const char *name)
{
	char *pname = g_malloc(BUF_LEN);
	GList *lc = log_conversations;
	struct log_conversation *l;


	strcpy(pname, normalize(name));

	while (lc) {
		l = (struct log_conversation *)lc->data;
		if (!g_strcasecmp(pname, normalize(l->name))) {
			g_free(pname);
			return l;
		}
		lc = lc->next;
	}
	g_free(pname);
	return NULL;
}

void update_log_convs()
{
	GSList *C = connections;
	struct gaim_connection *g;
	GSList *bcs;
	GList *cnv = conversations;
	struct conversation *c;

	while (cnv) {
		c = (struct conversation *)cnv->data;
		if (c->log_button) {
			if (c->is_chat)
				gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
						   ((logging_options & OPT_LOG_CHATS)) ? FALSE : TRUE);
			else
				gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
							 ((logging_options & OPT_LOG_CONVOS)) ? FALSE : TRUE);
		}

		cnv = cnv->next;
	}

	while (C) {
		g = (struct gaim_connection *)C->data;
		bcs = g->buddy_chats;
		while (bcs) {
			c = (struct conversation *)bcs->data;
			if (c->log_button) {
				if (c->is_chat)
					gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
					 		   ((logging_options & OPT_LOG_CHATS)) ? FALSE :
							   TRUE);
				else
					gtk_widget_set_sensitive(GTK_WIDGET(c->log_button),
								 ((logging_options & OPT_LOG_CONVOS)) ? FALSE :
								 TRUE);
			}

			bcs = bcs->next;
		}
		C = C->next;
	}
}

static void do_save_convo(GtkObject *obj, GtkWidget *wid)
{
	struct conversation *c = gtk_object_get_user_data(obj);
	const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(wid));
	FILE *f;
	if (file_is_dir(filename, wid))
		return;
	if (!((!c->is_chat && g_list_find(conversations, c)) ||
	      (c->is_chat && g_slist_find(connections, c->gc) && g_slist_find(c->gc->buddy_chats, c))))
 		filename = NULL;
	gtk_widget_destroy(wid);
	if (!filename)
		return;
	f = fopen(filename, "w+");
	if (!f)
		return;
	fprintf(f, "%s", c->history->str);
	fclose(f);
}


void save_convo(GtkWidget *save, struct conversation *c)
{
	char buf[BUF_LONG];
	GtkWidget *window = gtk_file_selection_new(_("Gaim - Save Conversation"));
	g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S "%s.log", gaim_home_dir(), normalize(c->name));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);
	gtk_object_set_user_data(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), c);
	g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
			   "clicked", G_CALLBACK(do_save_convo), window);
	g_signal_connect_swapped(GTK_OBJECT(GTK_FILE_SELECTION(window)->cancel_button),
				  "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer)window);
	gtk_widget_show(window);
}

char *html_logize(char *p)
{
	char *temp_p = p;
	char *buffer_p;
	char *buffer_start;
	int num_cr = 0;
	int char_len = 0;

	while (*temp_p != '\0') {
		char_len++;
		if ((*temp_p == '\n') || ((*temp_p == '<') && (*(temp_p + 1) == '!')))
			num_cr++;
		++temp_p;
	}

	temp_p = p;
	buffer_p = g_malloc(char_len + (4 * num_cr) + 1);
	buffer_start = buffer_p;

	while (*temp_p != '\0') {
		if (*temp_p == '\n') {
			*buffer_p++ = '<';
			*buffer_p++ = 'B';
			*buffer_p++ = 'R';
			*buffer_p++ = '>';
			*buffer_p++ = '\n';
		} else if ((*temp_p == '<') && (*(temp_p + 1) == '!')) {
			*buffer_p++ = '&';
			*buffer_p++ = 'l';
			*buffer_p++ = 't';
			*buffer_p++ = ';';
		} else
			*buffer_p++ = *temp_p;
		++temp_p;
	}
	*buffer_p = '\0';

	return buffer_start;
}

static FILE *open_gaim_log_file(const char *name, int *flag)
{
	char *buf;
	char *buf2;
	char log_all_file[256];
	struct stat st;
	FILE *fd;
#ifndef _WIN32
	int res;
#endif
	gchar *gaim_dir;

	buf = g_malloc(BUF_LONG);
	buf2 = g_malloc(BUF_LONG);
	gaim_dir = gaim_user_dir();

	/*  Dont log yourself */
	strncpy(log_all_file, gaim_dir, 256);

#ifndef _WIN32
	stat(log_all_file, &st);
	if (!S_ISDIR(st.st_mode))
		unlink(log_all_file);

	fd = fopen(log_all_file, "r");

	if (!fd) {
		res = mkdir(log_all_file, S_IRUSR | S_IWUSR | S_IXUSR);
		if (res < 0) {
			g_snprintf(buf, BUF_LONG, _("Unable to make directory %s for logging"),
				   log_all_file);
			do_error_dialog(buf, NULL, GAIM_ERROR);
			g_free(buf);
			g_free(buf2);
			return NULL;
		}
	} else
		fclose(fd);

	g_snprintf(log_all_file, 256, "%s" G_DIR_SEPARATOR_S "logs", gaim_dir);

	if (stat(log_all_file, &st) < 0)
		*flag = 1;
	if (!S_ISDIR(st.st_mode))
		unlink(log_all_file);

	fd = fopen(log_all_file, "r");
	if (!fd) {
		res = mkdir(log_all_file, S_IRUSR | S_IWUSR | S_IXUSR);
		if (res < 0) {
			g_snprintf(buf, BUF_LONG, _("Unable to make directory %s for logging"),
				   log_all_file);
			do_error_dialog(buf, NULL, GAIM_ERROR);
			g_free(buf);
			g_free(buf2);
			return NULL;
		}
	} else
		fclose(fd);
#else /* _WIN32 */
	g_snprintf(log_all_file, 256, "%s" G_DIR_SEPARATOR_S "logs", gaim_dir);

	if( _mkdir(log_all_file) < 0 && errno != EEXIST ) {
	  g_snprintf(buf, BUF_LONG, _("Unable to make directory %s for logging"), log_all_file);
	  do_error_dialog(buf, NULL, GAIM_ERROR);
	  g_free(buf);
	  g_free(buf2);
	  return NULL;
	}
#endif

	g_snprintf(log_all_file, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s", gaim_dir, name);
	if (stat(log_all_file, &st) < 0)
		*flag = 1;

	debug_printf("Logging to: \"%s\"\n", log_all_file);

	fd = fopen(log_all_file, "a");

	g_free(buf);
	g_free(buf2);
	return fd;
}

static FILE *open_system_log_file(char *name)
{
	int x;

	if (name)
		return open_log_file(name, 2);
	else
		return open_gaim_log_file("system", &x);
}

FILE *open_log_file(const char *name, int is_chat)
{
	struct stat st;
	char realname[256];
	struct log_conversation *l;
	FILE *fd;
	int flag = 0;

	if (((is_chat == 2) && !(logging_options & OPT_LOG_INDIVIDUAL))
		|| ((is_chat == 1) && !(logging_options & OPT_LOG_CHATS))
		|| ((is_chat == 0) && !(logging_options & OPT_LOG_CONVOS))) {

		l = find_log_info(name);
		if (!l)
			return NULL;

		if (stat(l->filename, &st) < 0)
			flag = 1;

		fd = fopen(l->filename, "a");

		if (flag) {	/* is a new file */
			if (logging_options & OPT_LOG_STRIP_HTML) {
				fprintf(fd, _("IM Sessions with %s\n"), name);
			} else {
				fprintf(fd, "<HTML><HEAD><TITLE>");
				fprintf(fd, _("IM Sessions with %s"), name);
				fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"ffffff\">\n");
			}
		}

		return fd;
	}

	g_snprintf(realname, sizeof(realname), "%s.log", normalize(name));
	fd = open_gaim_log_file(realname, &flag);

	if (fd && flag) {	/* is a new file */
		if (logging_options & OPT_LOG_STRIP_HTML) {
			fprintf(fd, _("IM Sessions with %s\n"), name);
		} else {
			fprintf(fd, "<HTML><HEAD><TITLE>");
			fprintf(fd, _("IM Sessions with %s"), name);
			fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"ffffff\">\n");
		}
	}

	return fd;
}

void system_log(enum log_event what, struct gaim_connection *gc,
				struct buddy *who, int why)
{
	FILE *fd;
	char text[256], html[256];

	if ((logging_options & why) != why)
		return;

	if (logging_options & OPT_LOG_INDIVIDUAL) {
		if (why & OPT_LOG_MY_SIGNON)
			fd = open_system_log_file(gc ? gc->username : NULL);
		else
			fd = open_system_log_file(who->name);
	} else
		fd = open_system_log_file(NULL);

	if (!fd)
		return;

	if (why & OPT_LOG_MY_SIGNON) {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) signed on @ %s"),
				   gc->username, gc->prpl->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) signed off @ %s"),
				   gc->username, gc->prpl->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) changed away state @ %s"),
				   gc->username, gc->prpl->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) came back @ %s"),
				   gc->username, gc->prpl->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) became idle @ %s"),
				   gc->username, gc->prpl->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) returned from idle @ %s"),
				   gc->username, gc->prpl->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_quit:
			g_snprintf(text, sizeof(text), _("+++ Program exit @ %s"), full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		}
	} else if (strcmp(who->name, who->show)) {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) signed on @ %s"),
				   gc->username, gc->prpl->name, who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) signed off @ %s"),
				   gc->username, gc->prpl->name, who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) went away @ %s"),
				   gc->username, gc->prpl->name, who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) came back @ %s"),
				   gc->username, gc->prpl->name, who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) became idle @ %s"),
				   gc->username, gc->prpl->name, who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text),
				   _("%s (%s) reported that %s (%s) returned from idle @ %s"), gc->username,
				   gc->prpl->name, who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		default:
			fclose(fd);
			return;
			break;
		}
	} else {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s signed on @ %s"),
				   gc->username, gc->prpl->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s signed off @ %s"),
				   gc->username, gc->prpl->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s went away @ %s"),
				   gc->username, gc->prpl->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s came back @ %s"),
				   gc->username, gc->prpl->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s became idle @ %s"),
				   gc->username, gc->prpl->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text),
				   _("%s (%s) reported that %s returned from idle @ %s"), gc->username,
				   gc->prpl->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		default:
			fclose(fd);
			return;
			break;
		}
	}

	if (logging_options & OPT_LOG_STRIP_HTML) {
		fprintf(fd, "---- %s ----\n", text);
	} else {
		if (logging_options & OPT_LOG_INDIVIDUAL)
			fprintf(fd, "<HR>%s<BR><HR><BR>\n", html);
		else
			fprintf(fd, "%s<BR>\n", html);
	}

	fclose(fd);
}
