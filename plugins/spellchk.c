/*
 * A lot of this code (especially the config code) was taken directly
 * or nearly directly from xchat, version 1.4.2 by Peter Zelezny and others.
 *
 * TODO:
 * 	? I think i did everything i want to with it.
 *
 * BUGS:
 * 	? I think i fixed them all.
 */
#define GAIM_PLUGINS
#include "gaim.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _WIN32
#include "win32dep.h"
#endif

struct replace_words {
	char *bad;
	char *good;
};

GList *words = NULL;

static int num_words(char *);
static int get_word(char *, int);
static char *have_word(char *, int);
static void substitute(char **, int, int, char *);

static void substitute_words(struct gaim_connection *gc, char *who, char **message, void *m) {
	int i, l;
	int word;
	GList *w;
	char *tmp;

	if (message == NULL || *message == NULL)
		return;

	l = num_words(*message);
	for (i = 0; i < l; i++) {
		word = get_word(*message, i);
		w = words;
		while (w) {
			struct replace_words *r;
			r = (struct replace_words *)(w->data);
			tmp = have_word(*message, word);
			if (!strcmp(tmp, r->bad)) {
				substitute(message, word, strlen(r->bad),
						r->good);
				l += num_words(r->good) - num_words(r->bad);
				i += num_words(r->good) - num_words(r->bad);
			}
			free(tmp);
			w = w->next;
		}
	}
}

static int buf_get_line(char *ibuf, char **buf, int *position, int len) {
	int pos = *position, spos = pos;

	if (pos == len)
		return 0;

	while (ibuf[pos++] != '\n') {
		if (pos == len)
			return 0;
	}
	pos--;
	ibuf[pos] = 0;
	*buf = &ibuf[spos];
	pos++;
	*position = pos;
	return 1;
}

static void load_conf() {
	char *defaultconf = "BAD r\nGOOD are\n\n"
			"BAD u\nGOOD you\n\n"
			"BAD teh\nGOOD the\n\n";
	char *buf, *ibuf;
	char name[82];
	char cmd[256];
	int fh, pnt = 0;
	struct stat st;

	if (words != NULL)
		g_list_free(words);
	words = NULL;

	buf = malloc(1000);
	snprintf(buf, 1000, "%s/.gaim/dict", gaim_home_dir());
	fh = open(buf, O_RDONLY, 0600);
	if (fh == -1) {
		fh = open(buf, O_TRUNC | O_WRONLY | O_CREAT, 0600);
		if (fh != -1) {
			write(fh, defaultconf, strlen(defaultconf));
			close(fh);
			free(buf);
			load_conf();
		}
		return;
	}
	free(buf);
	if (fstat(fh, &st) != 0)
		return;
	ibuf = malloc(st.st_size);
	read(fh, ibuf, st.st_size);
	close(fh);

	cmd[0] = 0;
	name[0] = 0;

	while(buf_get_line(ibuf, &buf, &pnt, st.st_size)) {
		if (*buf != '#') {
			if (!strncasecmp(buf, "BAD ", 4))
				strncpy(name, buf + 4, 81);
			if (!strncasecmp(buf, "GOOD ", 5)) {
				strncpy(cmd, buf + 5, 255);
				if (*name) {
					struct replace_words *r;
					r = malloc(sizeof *r);
					r->bad = strdup(name);
					r->good = strdup(cmd);
					words = g_list_append(words, r);
					cmd[0] = 0;
					name[0] = 0;
				}
			}
		}
	}
	free(ibuf);
}



static int num_words(char *m) {
	int count = 0;
	int pos;
	int state = 0;

	for (pos = 0; pos < strlen(m); pos++) {
		switch (state) {
		case 0: /* expecting word */
			if (!isspace(m[pos]) && !ispunct(m[pos])) {
				count++;
				state = 1;
			} else if (m[pos] == '<')
				state = 2;
			break;
		case 1: /* inside word */
			if (m[pos] == '<')
				state = 2;
			else if (isspace(m[pos]) || ispunct(m[pos]))
				state = 0;
			break;
		case 2: /* inside HTML tag */
			if (m[pos] == '>')
				state = 0;
			break;
		}
	}
	return count;
}

static int get_word(char *m, int word) {
	int count = 0;
	int pos = 0;
	int state = 0;

	for (pos = 0; pos < strlen(m) && count <= word; pos++) {
		switch (state) {
		case 0:
			if (!isspace(m[pos]) && !ispunct(m[pos])) {
				count++;
				state = 1;
			} else if (m[pos] == '<')
				state = 2;
			break;
		case 1:
			if (m[pos] == '<')
				state = 2;
			else if (isspace(m[pos]) || ispunct(m[pos]))
				state = 0;
			break;
		case 2:
			if (m[pos] == '>')
				state = 0;
			break;
		}
	}
	return pos - 1;
}

static char *have_word(char *m, int pos) {
	char *tmp = strpbrk(&m[pos], "' \t\f\r\n\"><.?!-,/");
	int len = (int)(tmp - &m[pos]);

	if (tmp == NULL) {
		tmp = strdup(&m[pos]);
		return tmp;
	}

	tmp = malloc(len + 1);
	tmp[0] = 0;
	strncat(tmp, &m[pos], len);

	return tmp;
}

static void substitute(char **mes, int pos, int m, char *text) {
	char *new = g_malloc(strlen(*mes) + strlen(text) + 1);
	char *tmp;
	new[0] = 0;

	strncat(new, *mes, pos);
	strcat(new, text);

	strcat(new, &(*mes)[pos + m]);
	tmp = *mes;
	*mes = new;
	g_free(tmp);
}

static GtkWidget *configwin = NULL;
static GtkWidget *list;
static GtkWidget *bad_entry;
static GtkWidget *good_entry;

static void row_unselect() {
	gtk_entry_set_text(GTK_ENTRY(bad_entry), "");
	gtk_entry_set_text(GTK_ENTRY(good_entry), "");
}

static void row_select() {
	char *badwrd, *goodwrd;
	int row;

	if (GTK_CLIST(list)->selection)
		row = (int) GTK_CLIST (list)->selection->data;
	else
		row = -1;
	if (row != -1) {
		gtk_clist_get_text(GTK_CLIST(list), row, 0, &badwrd);
		gtk_clist_get_text(GTK_CLIST(list), row, 1, &goodwrd);
		gtk_entry_set_text(GTK_ENTRY(bad_entry), badwrd);
		gtk_entry_set_text(GTK_ENTRY(good_entry), goodwrd);
	} else {
		row_unselect();
	}
}

static void list_add_new() {
	int i;
	gchar *item[2] = {"*NEW*", "EDIT ME"};

	i = gtk_clist_append(GTK_CLIST(list), item);
	gtk_clist_select_row(GTK_CLIST(list), i, 0);
	gtk_clist_moveto(GTK_CLIST(list), i, 0, 0.5, 0);
}

static void list_delete() {
	int row;

	if (GTK_CLIST(list)->selection)
		row = (int) GTK_CLIST (list)->selection->data;
	else
		row = -1;
	if (row != -1) {
		gtk_clist_unselect_all(GTK_CLIST(list));
		gtk_clist_remove(GTK_CLIST(list), row);
	}
}

static void close_config() {
	if (configwin)
		gtk_widget_destroy(configwin);
	configwin = NULL;
}

static void save_list() {
	int fh, i = 0;
	char buf[512];
	char *a, *b;

	snprintf(buf, sizeof buf, "%s/.gaim/dict", gaim_home_dir());
	fh = open(buf, O_TRUNC | O_WRONLY | O_CREAT, 0600);
	if (fh != 1) {
		while (gtk_clist_get_text(GTK_CLIST(list), i, 0, &a)) {
			gtk_clist_get_text(GTK_CLIST(list), i, 1, &b);
			snprintf(buf, sizeof buf, "BAD %s\nGOOD %s\n\n", a, b);
			write(fh, buf, strlen(buf));
			i++;
		}
		close (fh);
	}
	close_config();
	load_conf();
}

static void bad_changed() {
	int row;
	const char *m;

	if (GTK_CLIST(list)->selection)
		row = (int) GTK_CLIST (list)->selection->data;
	else
		row = -1;
	if (row != -1) {
		m = gtk_entry_get_text(GTK_ENTRY(bad_entry));
		gtk_clist_set_text(GTK_CLIST(list), row, 0, m);
	}
}

static void good_changed() {
	int row;
	const char *m;

	if (GTK_CLIST(list)->selection)
		row = (int) GTK_CLIST (list)->selection->data;
	else
		row = -1;
	if (row != -1) {
		m = gtk_entry_get_text(GTK_ENTRY(good_entry));
		gtk_clist_set_text(GTK_CLIST(list), row, 1, m);
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *gaim_plugin_init(GModule *handle) {
	load_conf();

	gaim_signal_connect(handle, event_im_send, substitute_words, NULL);
	gaim_signal_connect(handle, event_chat_send, substitute_words, NULL);
	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
}

struct gaim_plugin_description desc; 
G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup("Text replacement");
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup("Replaces text in outgoing messages according to user-defined rules.");
	desc.authors = g_strdup("Eric Warmehoven &lt;eric@warmenhoven.org>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}
 
G_MODULE_EXPORT char *name() {
	return "IM Spell Check";
}

G_MODULE_EXPORT char *description() {
	return "Watches outgoing IM text and corrects common spelling errors.";
}

G_MODULE_EXPORT GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *ret, *vbox, *win;
	GtkWidget *hbox;
	GtkWidget *button;
	GList *w = words;
	struct replace_words *r;
	char *pair[2] = {"Replace", "With"};

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);
	
	vbox = make_frame(ret, _("Text Replacements"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_container_add(GTK_CONTAINER(configwin), vbox);
	gtk_widget_show (vbox);

	win = gtk_scrolled_window_new(0, 0);
	gtk_container_add(GTK_CONTAINER(vbox), win);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (win),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_ALWAYS);
	gtk_widget_show(win);
	list = gtk_clist_new_with_titles(2, pair);
	gtk_clist_set_column_width(GTK_CLIST(list), 0, 90);
	gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_passive(GTK_CLIST(list));
	gtk_container_add(GTK_CONTAINER(win), list);
	g_signal_connect(GTK_OBJECT(list), "select_row",
			   G_CALLBACK(row_select), NULL);
	g_signal_connect(GTK_OBJECT(list), "unselect_row",
			   G_CALLBACK(row_unselect), NULL);
	gtk_widget_show(list);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, 0, 0, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_with_label("Add New");
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   G_CALLBACK(list_add_new), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	gtk_widget_set_usize(button, 100, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("Delete");
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   G_CALLBACK(list_delete), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	gtk_widget_set_usize(button, 100, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("Cancel");
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   G_CALLBACK(close_config), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	gtk_widget_set_usize(button, 100, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("Save");
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   G_CALLBACK(save_list), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	gtk_widget_set_usize(button, 100, 0);
	gtk_widget_show(button);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, 0, 0, 0);
	gtk_widget_show(hbox);

	bad_entry = gtk_entry_new_with_max_length(40);
	gtk_widget_set_usize(bad_entry, 96, 0);
	g_signal_connect(GTK_OBJECT(bad_entry), "changed",
			   G_CALLBACK(bad_changed), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), bad_entry, 0, 0, 0);
	gtk_widget_show(bad_entry);

	good_entry = gtk_entry_new_with_max_length(255);
	g_signal_connect(GTK_OBJECT(good_entry), "changed",
			   G_CALLBACK(good_changed), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), good_entry);
	gtk_widget_show(good_entry);

	while (w) {
		r = (struct replace_words *)(w->data);
		pair[0] = r->bad;
		pair[1] = r->good;
		gtk_clist_append(GTK_CLIST(list), pair);
		w = w->next;
	}

	gtk_widget_show_all(ret);
	return ret;
}
