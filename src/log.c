/* ---------------------------------------------------
 * Function to remove a log file entry
 * ---------------------------------------------------
 */
#include "internal.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "multi.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "util.h"

#include "gtkconv.h"

#include "ui.h"

GList *log_conversations = NULL;

void rm_log(struct log_conversation *a)
{
	GaimConversation *cnv = gaim_find_conversation(a->name);

	/* Added the following if statements for sanity check */
	if (!a)
	{
		gaim_notify_error (NULL, NULL, _("Error in specifying buddy conversation."), NULL);
		return;
	}
	cnv = gaim_find_conversation(a->name);
	if (!cnv)
	{
		gaim_notify_error (NULL, NULL, _("Unable to find conversation log"), NULL);
		return;
	}

	log_conversations = g_list_remove(log_conversations, a);
}

struct log_conversation *find_log_info(const char *name)
{
	char *pname = g_malloc(BUF_LEN);
	GList *lc = log_conversations;
	struct log_conversation *l;


	strcpy(pname, normalize(name));

	while (lc) {
		l = (struct log_conversation *)lc->data;
		if (!gaim_utf8_strcasecmp(pname, normalize(l->name))) {
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
	GList *cnv;
	GaimConversation *c;
	GaimGtkConversation *gtkconv;

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {

		c = (GaimConversation *)cnv->data;

		if (!GAIM_IS_GTK_CONVERSATION(c))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(c);

		if (gtkconv->toolbar.log) {
			if (gaim_conversation_get_type(c) == GAIM_CONV_CHAT)
				gtk_widget_set_sensitive(GTK_WIDGET(gtkconv->toolbar.log),
						!gaim_prefs_get_bool("/gaim/gtk/logging/log_chats"));
			else
				gtk_widget_set_sensitive(GTK_WIDGET(gtkconv->toolbar.log),
						!gaim_prefs_get_bool("/gaim/gtk/logging/log_ims"));
		}
	}
}

static void do_save_convo(GObject *obj, GtkWidget *wid)
{
	GaimConversation *c = g_object_get_data(obj, "gaim_conversation");
	const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(wid));
	FILE *f;

	if (file_is_dir(filename, wid))
		return;

	if (!((gaim_conversation_get_type(c) != GAIM_CONV_CHAT &&
		   g_list_find(gaim_get_ims(), c)) ||
		  (gaim_conversation_get_type(c) == GAIM_CONV_CHAT &&
		   g_list_find(gaim_get_chats(), c))))
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


void save_convo(GtkWidget *save, GaimConversation *c)
{
	char buf[BUF_LONG];
	GtkWidget *window = gtk_file_selection_new(_("Gaim - Save Conversation"));
	g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S "%s.log", gaim_home_dir(), normalize(c->name));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);
	g_object_set_data(G_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
			"gaim_conversation", c);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
			   "clicked", G_CALLBACK(do_save_convo), window);
	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(window)->cancel_button),
				  "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer)window);
	gtk_widget_show(window);
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
			gaim_notify_error(NULL, NULL, buf, NULL);
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
			gaim_notify_error(NULL, NULL, buf, NULL);
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
	  gaim_notify_error(NULL, NULL, buf, NULL);
	  g_free(buf);
	  g_free(buf2);
	  return NULL;
	}
#endif

	g_snprintf(log_all_file, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s", gaim_dir, name);
	if (stat(log_all_file, &st) < 0)
		*flag = 1;

	gaim_debug(GAIM_DEBUG_INFO, "log", "Logging to: \"%s\"\n", log_all_file);

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

	if (((is_chat == 2) && !gaim_prefs_get_bool("/gaim/gtk/logging/individual_logs"))
		|| ((is_chat == 1) && !gaim_prefs_get_bool("/gaim/gtk/logging/log_chats"))
		|| ((is_chat == 0) && !gaim_prefs_get_bool("/gaim/gtk/logging/log_ims"))) {

		l = find_log_info(name);
		if (!l)
			return NULL;

		if (stat(l->filename, &st) < 0)
			flag = 1;

		fd = fopen(l->filename, "a");

		if (flag) {	/* is a new file */
			if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html")) {
				fprintf(fd, _("IM Sessions with %s\n"), name);
			} else {
				fprintf(fd, "<HTML><HEAD><TITLE>");
				fprintf(fd, _("IM Sessions with %s"), name);
				fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"#ffffff\">\n");
			}
		}

		return fd;
	}

	g_snprintf(realname, sizeof(realname), "%s.log", normalize(name));
	fd = open_gaim_log_file(realname, &flag);

	if (fd && flag) {	/* is a new file */
		if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html")) {
			fprintf(fd, _("IM Sessions with %s\n"), name);
		} else {
			fprintf(fd, "<HTML><HEAD><TITLE>");
			fprintf(fd, _("IM Sessions with %s"), name);
			fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"#ffffff\">\n");
		}
	}

	return fd;
}

void system_log(enum log_event what, GaimConnection *gc,
				struct buddy *who, int why)
{
	GaimAccount *account;
	FILE *fd;
	char text[256], html[256];

	account = gaim_connection_get_account(gc);

	if ((why & OPT_LOG_MY_SIGNON &&
		 !gaim_prefs_get_bool("/gaim/gtk/logging/log_own_states")) ||
			(why & OPT_LOG_BUDDY_SIGNON &&
			 !gaim_prefs_get_bool("/gaim/gtk/logging/log_signon_signoff")) ||
			(why & OPT_LOG_BUDDY_IDLE &&
			 !gaim_prefs_get_bool("/gaim/gtk/logging/log_idle_state")) ||
			(why & OPT_LOG_BUDDY_AWAY &&
			 !gaim_prefs_get_bool("/gaim/gtk/logging/log_away_state"))) {

		return;
	}

	if (gaim_prefs_get_bool("/gaim/gtk/logging/individual_logs")) {
		if (why & OPT_LOG_MY_SIGNON)
			fd = open_system_log_file(gc ? (char *)gaim_account_get_username(account) : NULL);
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
				   gaim_account_get_username(account), gc->prpl->info->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) signed off @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) changed away state @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) came back @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) became idle @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text), _("+++ %s (%s) returned from idle @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_quit:
			g_snprintf(text, sizeof(text), _("+++ Program exit @ %s"), full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		}
	} else if (gaim_get_buddy_alias_only(who)) {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) signed on @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, gaim_get_buddy_alias(who), who->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) signed off @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, gaim_get_buddy_alias(who), who->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) went away @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, gaim_get_buddy_alias(who), who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) came back @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, gaim_get_buddy_alias(who), who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s (%s) became idle @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, gaim_get_buddy_alias(who), who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text),
				   _("%s (%s) reported that %s (%s) returned from idle @ %s"), gaim_account_get_username(account),
				   gc->prpl->info->name, gaim_get_buddy_alias(who), who->name, full_date());
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
				   gaim_account_get_username(account), gc->prpl->info->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s signed off @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s went away @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s came back @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), _("%s (%s) reported that %s became idle @ %s"),
				   gaim_account_get_username(account), gc->prpl->info->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text),
				   _("%s (%s) reported that %s returned from idle @ %s"), gaim_account_get_username(account),
				   gc->prpl->info->name, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		default:
			fclose(fd);
			return;
			break;
		}
	}

	if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
		fprintf(fd, "---- %s ----\n", text);
	else if (gaim_prefs_get_bool("/gaim/gtk/logging/individual_logs"))
		fprintf(fd, "<HR>%s<BR><HR><BR>\n", html);
	else
		fprintf(fd, "%s<BR>\n", html);

	fclose(fd);
}

char *html_logize(const char *p)
{
	const char *temp_p;
	char *buffer_p;
	char *buffer_start;
	int num_cr = 0;
	int char_len = 0;

	for (temp_p = p; *temp_p != '\0'; temp_p++) {
		char_len++;

		if ((*temp_p == '\n') || ((*temp_p == '<') && (*(temp_p + 1) == '!')))
			num_cr++;
	}

	buffer_p = g_malloc(char_len + (4 * num_cr) + 1);

	for (temp_p = p, buffer_start = buffer_p;
		 *temp_p != '\0';
		 temp_p++) {

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
	}

	*buffer_p = '\0';

	return buffer_start;
}
