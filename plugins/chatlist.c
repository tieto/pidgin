#include "config.h"

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#endif

#define GTK_ENABLE_BROKEN

#include "proxy.h"
#include "gaim.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "win32dep.h"
#endif

#define AOL_SRCHSTR "aim:GoChat?RoomName="

struct chat_page {
	GtkWidget *list1;
	GtkWidget *list2;
};

struct chat_room {
	char name[80];
	int exchange;
};

static GtkWidget *item = NULL; /* this is the parent tree */
static GList *chat_rooms = NULL;

static struct chat_page *cp = NULL;

static void des_item()
{
	if (item)
		gtk_widget_destroy(item);
	item = NULL;
}

static void handle_click_chat(GtkWidget *widget, GdkEventButton * event, struct chat_room *cr)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		GList *m = g_list_append(NULL, cr->name);
		int *x = g_new0(int, 1);
		*x = cr->exchange;
		m = g_list_append(m, x);
		serv_join_chat(connections->data, m);
		g_free(x);
		g_list_free(m);
	}
}

static void setup_buddy_chats()
{
	GList *crs = chat_rooms;
	GtkWidget *tree;

	if (!blist)
		return;

	if (item)
		gtk_tree_remove_item(GTK_TREE(buddies), item);
	item = NULL;

	if (!chat_rooms)
		return;

	item = gtk_tree_item_new_with_label(_("Buddy Chat"));
	g_signal_connect(GTK_OBJECT(item), "destroy", G_CALLBACK(des_item), NULL);
	gtk_tree_append(GTK_TREE(buddies), item);
	gtk_tree_item_expand(GTK_TREE_ITEM(item));
	gtk_widget_show(item);

	tree = gtk_tree_new();
	gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), tree);
	gtk_widget_show(tree);

	while (crs) {
		struct chat_room *cr = (struct chat_room *)crs->data;
		GtkWidget *titem = gtk_tree_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(titem), cr);
		gtk_tree_append(GTK_TREE(tree), titem);
		gtk_widget_show(titem);
		g_signal_connect(GTK_OBJECT(titem), "button_press_event",
				   G_CALLBACK(handle_click_chat), cr);
		crs = crs->next;
	}

	gtk_tree_item_expand(GTK_TREE_ITEM(item));
}

static void save_chat_prefs()
{
	FILE *f;
	char path[1024];
	char *x = gaim_user_dir();
	GList *crs = chat_rooms;

	g_snprintf(path, sizeof(path), "%s/%s", x, "chats");
	f = fopen(path, "w");
	if (!f)
		return;
	while (crs) {
		struct chat_room *cr = crs->data;
		crs = crs->next;
		fprintf(f, "%s\n%d\n", cr->name, cr->exchange);
	}
	fclose(f);
}

static void restore_chat_prefs()
{
	FILE *f;
	char path[1024];
	char *x = gaim_user_dir();
	char buf[1024];

	g_snprintf(path, sizeof(path), "%s/%s", x, "chats");
	f = fopen(path, "r");
	if (!f)
		return;
	while (fgets(buf, 1024, f)) {
		struct chat_room *cr = g_new0(struct chat_room, 1);
		g_snprintf(cr->name, sizeof(cr->name), "%s", g_strchomp(buf));
		if (!fgets(buf, 1024, f)) {
			g_free(cr);
			break;
		}
		cr->exchange = atoi(buf);
		chat_rooms = g_list_append(chat_rooms, cr);
	}
	fclose(f);
	setup_buddy_chats();
}

static void ref_list_callback(gpointer data, char *text, unsigned long len)
{
	char *c;
	GtkWidget *item;
	GList *items = GTK_LIST(cp->list1)->children;
	struct chat_room *cr;
	c = text;

	if (!text)
		return;

	while (items) {
		g_free(gtk_object_get_user_data(GTK_OBJECT(items->data)));
		items = items->next;
	}

	items = NULL;

	gtk_list_clear_items(GTK_LIST(cp->list1), 0, -1);

	item = gtk_list_item_new_with_label(_("Gaim Chat"));
	cr = g_new0(struct chat_room, 1);
	strcpy(cr->name, _("Gaim Chat"));
	cr->exchange = 4;
	gtk_object_set_user_data(GTK_OBJECT(item), cr);
	gtk_widget_show(item);

	items = g_list_append(NULL, item);

	while (c) {
		if (c - text > len - 30)
			break;	/* assume no chat rooms 30 from end, padding */
		if (!g_strncasecmp(AOL_SRCHSTR, c, strlen(AOL_SRCHSTR))) {
			char *t;
			int len = 0;
			int exchange = 5;
			char *name = NULL;

			c += strlen(AOL_SRCHSTR);
			t = c;
			while (t) {
				len++;
				name = g_realloc(name, len);
				if (*t == '+')
					name[len - 1] = ' ';
				else if (*t == '&') {
					name[len - 1] = 0;
					sscanf(t, "&Exchange=%d", &exchange);
					c = t + strlen("&Exchange=x");
					break;
				} else
					name[len - 1] = *t;
				t++;
			}
			cr = g_new0(struct chat_room, 1);
			strcpy(cr->name, name);
			cr->exchange = exchange;
			printf("Adding '%s'\n", name);
			item = gtk_list_item_new_with_label(name);
			gtk_widget_show(item);
			items = g_list_append(items, item);
			gtk_object_set_user_data(GTK_OBJECT(item), cr);
			g_free(name);
		}
		c++;
	}
	gtk_list_append_items(GTK_LIST(cp->list1), items);
}

static void refresh_list(GtkWidget *w, gpointer *m)
{
	grab_url("http://www.aim.com/community/chats.adp", FALSE, ref_list_callback, NULL);
}

static void add_chat(GtkWidget *w, gpointer *m)
{
	GList *sel = GTK_LIST(cp->list1)->selection;
	struct chat_room *cr, *cr2;
	GList *crs = chat_rooms;
	GtkWidget *item;

	if (sel) {
		cr = (struct chat_room *)gtk_object_get_user_data(GTK_OBJECT(sel->data));
	} else
		return;

	while (crs) {
		cr2 = (struct chat_room *)crs->data;
		if (!g_strcasecmp(cr->name, cr2->name))
			 return;
		crs = crs->next;
	}
	item = gtk_list_item_new_with_label(cr->name);
	cr2 = g_new0(struct chat_room, 1);
	strcpy(cr2->name, cr->name);
	cr2->exchange = cr->exchange;
	gtk_object_set_user_data(GTK_OBJECT(item), cr2);
	gtk_widget_show(item);
	sel = g_list_append(NULL, item);
	gtk_list_append_items(GTK_LIST(cp->list2), sel);
	chat_rooms = g_list_append(chat_rooms, cr2);

	setup_buddy_chats();
	save_chat_prefs();
}

static void remove_chat(GtkWidget *w, gpointer *m)
{
	GList *sel = GTK_LIST(cp->list2)->selection;
	struct chat_room *cr;
	GList *crs;
	GtkWidget *item;

	if (sel) {
		item = (GtkWidget *)sel->data;
		cr = (struct chat_room *)gtk_object_get_user_data(GTK_OBJECT(item));
	} else
		return;

	chat_rooms = g_list_remove(chat_rooms, cr);


	gtk_list_clear_items(GTK_LIST(cp->list2), 0, -1);

	if (g_list_length(chat_rooms) == 0)
		chat_rooms = NULL;

	crs = chat_rooms;

	while (crs) {
		cr = (struct chat_room *)crs->data;
		item = gtk_list_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(item), cr);
		gtk_widget_show(item);
		gtk_list_append_items(GTK_LIST(cp->list2), g_list_append(NULL, item));


		crs = crs->next;
	}

	setup_buddy_chats();
	save_chat_prefs();
}

G_MODULE_EXPORT GtkWidget *gaim_plugin_config_gtk()
{
	GtkWidget *ret, *vbox;
	GtkWidget *list1, *list2;
	GtkWidget *sw1, *sw2;
	GtkWidget *ref_button, *add_button, *rem_button;
	GtkWidget *table, *label;
	struct chat_room *cr = NULL;
	GList *items = NULL;
	GList *crs = chat_rooms;
	
	if (cp)
                g_free(cp);
        cp = g_new0(struct chat_page, 1);


	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame(ret, _("Chat Rooms"));

	table = gtk_table_new(4, 2, FALSE);

	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	list1 = gtk_list_new();
	list2 = gtk_list_new();
	sw1 = gtk_scrolled_window_new(NULL, NULL);
	sw2 = gtk_scrolled_window_new(NULL, NULL);

	ref_button = gaim_pixbuf_button_from_stock(_("Refresh"), GTK_STOCK_REFRESH, GAIM_BUTTON_HORIZONTAL);
	add_button = gaim_pixbuf_button_from_stock(_("Add"), GTK_STOCK_ADD, GAIM_BUTTON_HORIZONTAL);
	rem_button = gaim_pixbuf_button_from_stock(_("Remove"), GTK_STOCK_REMOVE, GAIM_BUTTON_HORIZONTAL);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw1), list1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list2);

	cp->list1 = list1;
	cp->list2 = list2;

	g_signal_connect(GTK_OBJECT(ref_button), "clicked", G_CALLBACK(refresh_list), cp);
	g_signal_connect(GTK_OBJECT(rem_button), "clicked", G_CALLBACK(remove_chat), cp);
	g_signal_connect(GTK_OBJECT(add_button), "clicked", G_CALLBACK(add_chat), cp);

	label = gtk_label_new(_("List of available chats"));

	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), ref_button, 0, 1, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), sw1, 0, 1, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach(GTK_TABLE(table), add_button, 0, 1, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0);


	label = gtk_label_new(_("List of subscribed chats"));

	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), sw2, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach(GTK_TABLE(table), rem_button, 1, 2, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0);


	item = gtk_list_item_new_with_label(_("Gaim Chat"));
	cr = g_new0(struct chat_room, 1);
	strcpy(cr->name, _("Gaim Chat"));
	cr->exchange = 4;
	gtk_object_set_user_data(GTK_OBJECT(item), cr);
	gtk_list_append_items(GTK_LIST(list1), g_list_append(NULL, item));


	while (crs) {
		cr = (struct chat_room *)crs->data;
		item = gtk_list_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(item), cr);
		items = g_list_append(items, item);

		crs = crs->next;
	}

	gtk_list_append_items(GTK_LIST(list2), items);
	gtk_widget_show_all(ret);
	return ret;
}

static void handle_signon(struct gaim_connection *gc)
{
	setup_buddy_chats();
}

G_MODULE_EXPORT char *gaim_plugin_init(GModule *m)
{
	restore_chat_prefs();
	gaim_signal_connect(m, event_signon, handle_signon, NULL);
	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove()
{
	if (item)
		gtk_tree_remove_item(GTK_TREE(buddies), item);
	item = NULL;

	while (chat_rooms) {
		g_free(chat_rooms->data);
		chat_rooms = g_list_remove(chat_rooms, chat_rooms->data);
	}

	if (cp)
		g_free(cp);
	cp = NULL;
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = GAIM_PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Chat List"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Allows you to add chat rooms to your buddy list."));
	desc.authors = g_strdup("Eric Warmenhoven &lt;eric@warmenhoven.org>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

G_MODULE_EXPORT char *name()
{
	return _("Chat List");
}

G_MODULE_EXPORT char *description()
{
	return _("Allows you to add chat rooms to your buddy list. Click the configure button to choose"
		" which rooms.");
}
