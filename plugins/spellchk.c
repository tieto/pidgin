/*
 * A lot of this code (especially the config code) was taken directly
 * or nearly directly from xchat, version 1.4.2 by Peter Zelezny and others.
 */
#include "internal.h"
#include "gtkgaim.h"

#include "debug.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkplugin.h"
#include "gtkutils.h"

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#define SPELLCHECK_PLUGIN_ID "gtk-spellcheck"

enum {
	BAD_COLUMN,
	GOOD_COLUMN,
	N_COLUMNS
};

static int num_words(const char *);
static int get_word(char *, int);
static char *have_word(char *, int);
static void substitute(char **, int, int, const char *);
static GtkListStore *model;

static gboolean
substitute_words(GaimAccount *account, GaimConversation *conv,
				 char **message, void *data)
{
	int i, l;
	int word;
	char *tmp;

	if (message == NULL || *message == NULL)
		return FALSE;

	l = num_words(*message);
	for (i = 0; i < l; i++) {
		GtkTreeIter iter;
		word = get_word(*message, i);
		if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
			do {
				GValue val0, val1;
				const char *bad, *good;
				memset(&val0, 0, sizeof(val0));
				memset(&val1, 0, sizeof(val1));
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 0, &val0);
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 1, &val1);
				tmp = have_word(*message, word);
				bad = g_value_get_string(&val0);
				good = g_value_get_string(&val1);
				if (!strcmp(tmp, bad)) {
					substitute(message, word, strlen(bad),
							good);
					l += num_words(good) - num_words(bad);
					i += num_words(good) - num_words(bad);
				}
				free(tmp);
				g_value_unset(&val0);
				g_value_unset(&val1);
			} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
		}
	}

	return FALSE;
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
	const char * const defaultconf = "BAD r\nGOOD are\n\n"
			"BAD u\nGOOD you\n\n"
			"BAD teh\nGOOD the\n\n";
	gchar *buf, *ibuf;
	char name[82];
	char cmd[256];
	int pnt = 0;
	gsize size;

	model = gtk_list_store_new((gint)N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	buf = g_build_filename(gaim_user_dir(), "dict", NULL);
	g_file_get_contents(buf, &ibuf, &size, NULL);
	g_free(buf);
	if(!ibuf) {
		ibuf = g_strdup(defaultconf);
		size = strlen(defaultconf);
	}

	cmd[0] = 0;
	name[0] = 0;

	while(buf_get_line(ibuf, &buf, &pnt, size)) {
		if (*buf != '#') {
			if (!strncasecmp(buf, "BAD ", 4))
				strncpy(name, buf + 4, 81);
			if (!strncasecmp(buf, "GOOD ", 5)) {
				strncpy(cmd, buf + 5, 255);
				if (*name) {
					GtkTreeIter iter;
					gtk_list_store_append(model, &iter);
					gtk_list_store_set(model, &iter,
						0, g_strdup(name),
						1, g_strdup(cmd),
						-1);
				}
			}
		}
	}
	g_free(ibuf);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
	                                     0, GTK_SORT_ASCENDING);
}



static int num_words(const char *m) {
	int count = 0;
	guint pos;
	int state = 0;

	for (pos = 0; pos < strlen(m); pos++) {
		switch (state) {
		case 0: /* expecting word */
			if (!g_ascii_isspace(m[pos]) && !g_ascii_ispunct(m[pos])) {
				count++;
				state = 1;
			} else if (m[pos] == '<')
				state = 2;
			break;
		case 1: /* inside word */
			if (m[pos] == '<')
				state = 2;
			else if (g_ascii_isspace(m[pos]) || g_ascii_ispunct(m[pos]))
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
	guint pos = 0;
	int state = 0;

	for (pos = 0; pos < strlen(m) && count <= word; pos++) {
		switch (state) {
		case 0:
			if (!g_ascii_isspace(m[pos]) && !g_ascii_ispunct(m[pos])) {
				count++;
				state = 1;
			} else if (m[pos] == '<')
				state = 2;
			break;
		case 1:
			if (m[pos] == '<')
				state = 2;
			else if (g_ascii_isspace(m[pos]) || g_ascii_ispunct(m[pos]))
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

static void substitute(char **mes, int pos, int m, const char *text) {
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

static GtkWidget *tree;
static GtkWidget *bad_entry;
static GtkWidget *good_entry;

static void save_list();

static void on_edited(GtkCellRendererText *cellrenderertext,
					  gchar *path, gchar *arg2, gpointer data)
{
	GtkTreeIter iter;
	GValue val;
	if(arg2[0] == '\0') {
		gdk_beep();
		return;
	}
	memset(&val, 0, sizeof(val));
	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, GPOINTER_TO_INT(data), &val);
	if(strcmp(arg2, g_value_get_string(&val))) {
		gtk_list_store_set(model, &iter, GPOINTER_TO_INT(data), arg2, -1);
		save_list();
	}
	g_value_unset(&val);
}

static void list_add_new()
{
	GtkTreeIter iter;

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
		0, gtk_entry_get_text(GTK_ENTRY(bad_entry)),
		1, gtk_entry_get_text(GTK_ENTRY(good_entry)),
		-1);
	gtk_editable_delete_text(GTK_EDITABLE(bad_entry), 0, -1);
	gtk_editable_delete_text(GTK_EDITABLE(good_entry), 0, -1);
	gtk_widget_grab_focus(bad_entry);

	save_list();
}

static void add_selected_row_to_list(GtkTreeModel *model, GtkTreePath *path,
	GtkTreeIter *iter, gpointer data)
{
	GSList **list = (GSList **)data;
	*list = g_slist_append(*list, gtk_tree_path_copy(path) );
}


static void remove_row(void *data1, gpointer data2)
{
	GtkTreePath *path = (GtkTreePath*)data1;
	GtkTreeIter iter;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_list_store_remove(model, &iter);
	gtk_tree_path_free(path);
}

static void list_delete()
{
	GtkTreeSelection *sel;
	GSList *list = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_selected_foreach(sel, add_selected_row_to_list, &list);

	g_slist_foreach(list, remove_row, NULL);
	g_slist_free(list);

	save_list();
}

static void save_list()
{
	FILE *f;
	char *name;
	GtkTreeIter iter;
	char tempfilename[BUF_LONG];
	int fd;

	name = g_build_filename(gaim_user_dir(), "dict", NULL);
	strcpy(tempfilename, name);
	strcat(tempfilename,".XXXXXX");
	fd = g_mkstemp(tempfilename);
	if(fd<0) {
		perror(tempfilename);
		g_free(name);
		return;
	}
	if (!(f = fdopen(fd, "w"))) {
		perror("fdopen");
		close(fd);
		g_free(name);
		return;
	}

	fchmod(fd, S_IRUSR | S_IWUSR);

	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
		do {
			GValue val0, val1;
			memset(&val0, 0, sizeof(val0));
			memset(&val1, 0, sizeof(val1));
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 0, &val0);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 1, &val1);
			fprintf(f, "BAD %s\nGOOD %s\n\n", g_value_get_string(&val0), g_value_get_string(&val1));
			g_value_unset(&val0);
			g_value_unset(&val1);
		} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
	}
	if(fclose(f)) {
		gaim_debug(GAIM_DEBUG_ERROR, "spellchk",
				   "Error writing to %s: %m\n", tempfilename);
		g_unlink(tempfilename);
		g_free(name);
		return;
	}
	g_rename(tempfilename, name);
	g_free(name);
}

static void
check_if_something_is_selected(GtkTreeModel *model,
	GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	*((gboolean*)data) = TRUE;
}

static void on_selection_changed(GtkTreeSelection *sel,
	gpointer data)
{
	gboolean is = FALSE;
	gtk_tree_selection_selected_foreach(sel, check_if_something_is_selected, &is);
	gtk_widget_set_sensitive((GtkWidget*)data, is);
}

static gboolean non_empty(const char *s)
{
	while(*s && g_ascii_isspace(*s))
		s++;
	return *s;
}

static void on_entry_changed(GtkEditable *editable, gpointer data)
{
	gtk_widget_set_sensitive((GtkWidget*)data,
		non_empty(gtk_entry_get_text(GTK_ENTRY(bad_entry))) &&
		non_empty(gtk_entry_get_text(GTK_ENTRY(good_entry))));
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *conv_handle = gaim_conversations_get_handle();

	load_conf();

	gaim_signal_connect(conv_handle, "writing-im-msg",
						plugin, GAIM_CALLBACK(substitute_words), NULL);
	gaim_signal_connect(conv_handle, "writing-chat-msg",
						plugin, GAIM_CALLBACK(substitute_words), NULL);

	return TRUE;
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret, *vbox, *win;
	GtkWidget *hbox, *label;
	GtkWidget *button;
	GtkSizeGroup *sg;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = gaim_gtk_make_frame(ret, _("Text Replacements"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_widget_set_size_request(vbox, 300, -1);
	gtk_widget_show (vbox);

	win = gtk_scrolled_window_new(0, 0);
	gtk_container_add(GTK_CONTAINER(vbox), win);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(win),
										GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (win),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
	gtk_widget_show(win);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	/* gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE); */
	gtk_widget_set_size_request(tree, 260,200);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer),
		"editable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
			   G_CALLBACK(on_edited), GINT_TO_POINTER(0));
	column = gtk_tree_view_column_new_with_attributes (_("You type"),
		renderer, "text", BAD_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 130);
	/* gtk_tree_view_column_set_resizable(column, TRUE); */
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer),
		"editable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
			   G_CALLBACK(on_edited), GINT_TO_POINTER(1));
	column = gtk_tree_view_column_new_with_attributes (_("You send"),
		renderer, "text", GOOD_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 130);
	/* gtk_tree_view_column_set_resizable(column, TRUE); */
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)),
		 GTK_SELECTION_MULTIPLE);
	gtk_container_add(GTK_CONTAINER(win), tree);
	gtk_widget_show(tree);

	hbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK(list_delete), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);

	g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree))),
		"changed", G_CALLBACK(on_selection_changed), button);

	gtk_widget_show(button);

	vbox = gaim_gtk_make_frame(ret, _("Add a new text replacement"));
	gtk_widget_set_size_request(vbox, 300, -1);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("You _type:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	bad_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(bad_entry), 40);
	gtk_box_pack_start(GTK_BOX(hbox), bad_entry, TRUE, TRUE, 0);
	gtk_size_group_add_widget(sg, bad_entry);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), bad_entry);
	gtk_widget_show(bad_entry);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("You _send:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	good_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(good_entry), 255);
	gtk_box_pack_start(GTK_BOX(hbox), good_entry, TRUE, TRUE, 0);
	gtk_size_group_add_widget(sg, good_entry);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), good_entry);
	gtk_widget_show(good_entry);

	hbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK(list_add_new), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(bad_entry), "changed", G_CALLBACK(on_entry_changed), button);
	g_signal_connect(G_OBJECT(good_entry), "changed", G_CALLBACK(on_entry_changed), button);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);


	gtk_widget_show_all(ret);
	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	SPELLCHECK_PLUGIN_ID,
	N_("Text replacement"),
	VERSION,
	N_("Replaces text in outgoing messages according to user-defined rules."),
	N_("Replaces text in outgoing messages according to user-defined rules."),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	GAIM_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	&ui_info,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(spellcheck, init_plugin, info)
