/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#define _PIDGIN_GTKDIALOGS_C_

#include "internal.h"
#include "pidgin.h"
#include "package_revision.h"
#ifdef HAVE_MESON_CONFIG
#include "meson-config.h"
#endif

#include "debug.h"
#include "notify.h"
#include "plugins.h"
#include "protocol.h"
#include "request.h"
#include "util.h"
#include "core.h"

#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtklog.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidginstock.h"

#ifdef USE_GSTREAMER
#include <gst/gst.h>
#endif

#include "gtk3compat.h"

static GList *dialogwindows = NULL;

struct _PidginGroupMergeObject {
	PurpleGroup* parent;
	char *new_name;
};

void
pidgin_dialogs_destroy_all()
{
	while (dialogwindows) {
		gtk_widget_destroy(dialogwindows->data);
		dialogwindows = g_list_remove(dialogwindows, dialogwindows->data);
	}
}

static void destroy_win(GtkWidget *button, GtkWidget *win)
{
	gtk_widget_destroy(win);
}

#if 0
/* This function puts the version number onto the pixmap we use in the 'about'
 * screen in Pidgin. */
static void
pidgin_logo_versionize(GdkPixbuf **original, GtkWidget *widget) {
	GdkPixmap *pixmap;
	GtkStyle *style;
	PangoContext *context;
	PangoLayout *layout;
	gchar *markup;
	gint width, height;
	gint lwidth = 0, lheight = 0;

	style = gtk_widget_get_style(widget);

	gdk_pixbuf_render_pixmap_and_mask(*original, &pixmap, NULL, 255);
	width = gdk_pixbuf_get_width(*original);
	height = gdk_pixbuf_get_height(*original);
	g_object_unref(G_OBJECT(*original));

	context = gtk_widget_get_pango_context(widget);
	layout = pango_layout_new(context);

	markup = g_strdup_printf("<span foreground=\"#000000\">%s</span>", DISPLAY_VERSION);
	pango_layout_set_font_description(layout, style->font_desc);
	pango_layout_set_markup(layout, markup, strlen(markup));
	g_free(markup);

	pango_layout_get_pixel_size(layout, &lwidth, &lheight);
	gdk_draw_layout(GDK_DRAWABLE(pixmap), style->bg_gc[GTK_STATE_NORMAL],
					width - (lwidth + 3), 1, layout);
	g_object_unref(G_OBJECT(layout));

	*original = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL,
											 0, 0, 0, 0,
											 width, height);
	g_object_unref(G_OBJECT(pixmap));
}
#endif

/* Note: Frees 'string' */
static GtkWidget *
pidgin_build_help_dialog(const char *title, const char *role, GString *string)
{
	GtkWidget *win, *vbox, *frame, *logo, *webview, *button;
	GdkPixbuf *pixbuf;
	AtkObject *obj;
	char *filename, *tmp;

	win = pidgin_create_dialog(title, 0, role, TRUE);
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(win), FALSE, 0);
	gtk_window_set_default_size(GTK_WINDOW(win), 475, 450);

	/* Generate a logo with a version number */
	filename = g_build_filename(PURPLE_DATADIR,
		"pixmaps", "pidgin", "logo.png", NULL);
	pixbuf = pidgin_pixbuf_new_from_file(filename);
	g_free(filename);

#if 0  /* Don't versionize the logo when the logo has the version in it */
	pidgin_logo_versionize(&pixbuf, logo);
#endif

	/* Insert the logo */
	logo = gtk_image_new_from_pixbuf(pixbuf);
	if (pixbuf)
		g_object_unref(G_OBJECT(pixbuf));
	obj = gtk_widget_get_accessible(logo);
	tmp = g_strconcat(PIDGIN_NAME, " " DISPLAY_VERSION, NULL);
	atk_object_set_description(obj, tmp);
	g_free(tmp);
	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 0);

	frame = pidgin_create_webview(FALSE, &webview, NULL);
	pidgin_webview_set_format_functions(PIDGIN_WEBVIEW(webview), PIDGIN_WEBVIEW_ALL ^ PIDGIN_WEBVIEW_SMILEY);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	pidgin_webview_append_html(PIDGIN_WEBVIEW(webview), string->str);

	button = pidgin_dialog_add_button(GTK_DIALOG(win), GTK_STOCK_CLOSE,
	                G_CALLBACK(destroy_win), win);

	gtk_widget_set_can_default(button, TRUE);
	gtk_widget_grab_default(button);

	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));

	g_string_free(string, TRUE);

	return win;
}

void pidgin_dialogs_plugins_info(void)
{
	GString *str;
	GList *plugins, *l = NULL;
	PurplePlugin *plugin = NULL;
	PurplePluginInfo *info;
	PurplePluginExtraCb extra_cb;
	char *title = g_strdup_printf(_("%s Plugin Information"), PIDGIN_NAME);
	char *pname = NULL, *authors, *pauthors, *pextra;
	const char *pver, *plicense, *pwebsite, *pid;
	gboolean ploaded, ploadable;
	const char * const *authorlist;
	guint n_authors;
	static GtkWidget *plugins_info = NULL;

	str = g_string_sized_new(4096);

	g_string_append_printf(str, "<h2>%s</h2><dl>", _("Plugin Information"));

	plugins = purple_plugins_find_all();

	for(l = plugins; l; l = l->next) {
		plugin = PURPLE_PLUGIN(l->data);
		info = purple_plugin_get_info(plugin);
		extra_cb = purple_plugin_info_get_extra_cb(info);

		pname = g_markup_escape_text(purple_plugin_info_get_name(info), -1);
		authorlist = purple_plugin_info_get_authors(info);

		if (authorlist) {
			authors = g_strjoinv(", ", (gchar **)authorlist);
			n_authors = g_strv_length((gchar **)authorlist);
		} else {
			authors = NULL;
			n_authors = 0;
		}

		if (authors)
			pauthors = g_markup_escape_text(authors, -1);
		else
			pauthors = NULL;

		pver = purple_plugin_info_get_version(info);
		plicense = purple_plugin_info_get_license_id(info);
		pwebsite = purple_plugin_info_get_website(info);
		pid = purple_plugin_info_get_id(info);
		ploadable = !purple_plugin_info_get_error(info);
		ploaded = purple_plugin_is_loaded(plugin);

		if (ploaded && extra_cb)
			pextra = extra_cb(plugin);
		else
			pextra = NULL;

		g_string_append_printf(str, "<dt>%s</dt><dd>", pname);
		if (pauthors)
			g_string_append_printf(str, "<b>%s:</b> %s<br/>",
				(n_authors > 1 ? "Authors" : "Author"), pauthors ? pauthors : "");
		g_string_append_printf(str,
				"<b>Version:</b> %s<br/>"
				"<b>License:</b> %s<br/>"
				"<b>Website:</b> %s<br/>"
				"<b>ID String:</b> %s<br/>"
				"<b>Extra:</b> %s<br/>"
				"<b>Loadable:</b> %s<br/>"
				"<b>Loaded:</b> %s"
				"</dd><br/>",
				pver     ? pver     : "",
				plicense ? plicense : "",
				pwebsite ? pwebsite : "",
				pid,
				pextra    ? pextra  : "",
				ploadable ? "Yes" : "<span style=\"color: #FF0000;\"><b>No</b></span>",
				ploaded   ? "Yes" : "No");

		g_free(pname);
		g_free(pextra);
		g_free(pauthors);
		g_free(authors);
	}
	g_list_free(plugins);

	g_string_append(str, "</dl>");

	plugins_info = pidgin_build_help_dialog(title, "plugins_info", str);
	g_signal_connect(G_OBJECT(plugins_info), "destroy",
			G_CALLBACK(gtk_widget_destroyed), &plugins_info);
	g_free(title);
}

static void
pidgin_dialogs_im_cb(gpointer data, PurpleRequestFields *fields)
{
	PurpleAccount *account;
	const char *username;

	account  = purple_request_fields_get_account(fields, "account");
	username = purple_request_fields_get_string(fields,  "screenname");

	pidgin_dialogs_im_with_user(account, username);
}

static gboolean
pidgin_dialogs_im_name_validator(PurpleRequestField *field, gchar **errmsg,
	void *_fields)
{
	PurpleRequestFields *fields = _fields;
	PurpleAccount *account;
	PurpleProtocol *protocol;
	const char *username;
	gboolean valid;

	account = purple_request_fields_get_account(fields, "account");
	protocol = purple_protocols_find(purple_account_get_protocol_id(account));
	username = purple_request_fields_get_string(fields, "screenname");

	valid = purple_validate(protocol, username);

	if (errmsg && !valid)
		*errmsg = g_strdup(_("Invalid username"));

	return valid;
}

void
pidgin_dialogs_im(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_validator(field, pidgin_dialogs_im_name_validator, fields);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("_Account"), NULL);
	purple_request_field_set_type_hint(field, "account");
	purple_request_field_set_visible(field,
		(purple_connections_get_all() != NULL &&
		 purple_connections_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_blist_get_buddy_list(), _("New Instant Message"),
						NULL,
						_("Please enter the username or alias of the person "
						  "you would like to IM."),
						fields,
						_("OK"), G_CALLBACK(pidgin_dialogs_im_cb),
						_("Cancel"), NULL,
						NULL, NULL);
}

void
pidgin_dialogs_im_with_user(PurpleAccount *account, const char *username)
{
	PurpleIMConversation *im;

	g_return_if_fail(account != NULL);
	g_return_if_fail(username != NULL);

	im = purple_conversations_find_im_with_account(username, account);

	if (im == NULL)
		im = purple_im_conversation_new(account, username);

	pidgin_conv_attach_to_conversation(PURPLE_CONVERSATION(im));
	purple_conversation_present(PURPLE_CONVERSATION(im));
}

static gboolean
pidgin_dialogs_ee(const char *ee)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	gchar *norm = purple_strreplace(ee, "rocksmyworld", "");

	label = gtk_label_new(NULL);
	if (purple_strequal(norm, "zilding"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"purple\">Amazing!  Simply Amazing!</span>");
	else if (purple_strequal(norm, "robflynn"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#1f6bad\">Pimpin\' Penguin Style! *Waddle Waddle*</span>");
	else if (purple_strequal(norm, "flynorange"))
		gtk_label_set_markup(GTK_LABEL(label),
				      "<span weight=\"bold\" size=\"large\" foreground=\"blue\">You should be me.  I'm so cute!</span>");
	else if (purple_strequal(norm, "ewarmenhoven"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"orange\">Now that's what I like!</span>");
	else if (purple_strequal(norm, "markster97"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"brown\">Ahh, and excellent choice!</span>");
	else if (purple_strequal(norm, "seanegn"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#009900\">Everytime you click my name, an angel gets its wings.</span>");
	else if (purple_strequal(norm, "chipx86"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"red\">This sunflower seed taste like pizza.</span>");
	else if (purple_strequal(norm, "markdoliner"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"#6364B1\">Hey!  I was in that tumbleweed!</span>");
	else if (purple_strequal(norm, "lschiere"))
		gtk_label_set_markup(GTK_LABEL(label),
				     "<span weight=\"bold\" size=\"large\" foreground=\"gray\">I'm not anything.</span>");
	g_free(norm);

	if (strlen(gtk_label_get_label(GTK_LABEL(label))) <= 0)
		return FALSE;

	window = gtk_dialog_new_with_buttons(PIDGIN_ALERT_TITLE, NULL, 0, GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_container_set_border_width (GTK_CONTAINER(window), PIDGIN_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(window))),
	                    PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(window))),
	                               PIDGIN_HIG_BOX_SPACE);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(window))), hbox);
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_COOL, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(window);
	return TRUE;
}

static void
pidgin_dialogs_info_cb(gpointer data, PurpleRequestFields *fields)
{
	char *username;
	gboolean found = FALSE;
	PurpleAccount *account;

	account  = purple_request_fields_get_account(fields, "account");

	username = g_strdup(purple_normalize(account,
		purple_request_fields_get_string(fields,  "screenname")));

	if (username != NULL && purple_str_has_suffix(username, "rocksmyworld"))
		found = pidgin_dialogs_ee(username);

	if (!found && username != NULL && *username != '\0' && account != NULL)
		pidgin_retrieve_user_info(purple_account_get_connection(account), username);

	g_free(username);
}

void
pidgin_dialogs_info(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("_Account"), NULL);
	purple_request_field_set_type_hint(field, "account");
	purple_request_field_set_visible(field,
		(purple_connections_get_all() != NULL &&
		 purple_connections_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_blist_get_buddy_list(), _("Get User Info"),
						NULL,
						_("Please enter the username or alias of the person "
						  "whose info you would like to view."),
						fields,
						_("OK"), G_CALLBACK(pidgin_dialogs_info_cb),
						_("Cancel"), NULL,
						NULL, NULL);
}

static void
pidgin_dialogs_log_cb(gpointer data, PurpleRequestFields *fields)
{
	char *username;
	PurpleAccount *account;
	GSList *cur;

	account  = purple_request_fields_get_account(fields, "account");

	username = g_strdup(purple_normalize(account,
		purple_request_fields_get_string(fields,  "screenname")));

	if (username != NULL && *username != '\0' && account != NULL)
	{
		PidginBuddyList *gtkblist = pidgin_blist_get_default_gtk_blist();
		GSList *buddies;

		pidgin_set_cursor(gtkblist->window, GDK_WATCH);

		buddies = purple_blist_find_buddies(account, username);
		for (cur = buddies; cur != NULL; cur = cur->next)
		{
			PurpleBlistNode *node = cur->data;
			if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
			{
				pidgin_log_show_contact((PurpleContact *)node->parent);
				g_slist_free(buddies);
				pidgin_clear_cursor(gtkblist->window);
				g_free(username);
				return;
			}
		}
		g_slist_free(buddies);

		pidgin_log_show(PURPLE_LOG_IM, username, account);

		pidgin_clear_cursor(gtkblist->window);
	}

	g_free(username);
}

/*
 * TODO - This needs to deal with logs of all types, not just IM logs.
 */
void
pidgin_dialogs_log(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname-all");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("_Account"), NULL);

	/* purple_request_field_account_new() only sets a default value if you're
	 * connected, and it sets it from the list of connected accounts.  Since
	 * we're going to set show_all here, it makes sense to use the first
	 * account, not the first connected account. */
	if (purple_accounts_get_all() != NULL) {
		purple_request_field_account_set_default_value(field, purple_accounts_get_all()->data);
		purple_request_field_account_set_value(field, purple_accounts_get_all()->data);
	}

	purple_request_field_set_type_hint(field, "account");
	purple_request_field_account_set_show_all(field, TRUE);
	purple_request_field_set_visible(field,
		(purple_accounts_get_all() != NULL &&
		 purple_accounts_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_blist_get_buddy_list(), _("View User Log"),
						NULL,
						_("Please enter the username or alias of the person "
						  "whose log you would like to view."),
						fields,
						_("OK"), G_CALLBACK(pidgin_dialogs_log_cb),
						_("Cancel"), NULL,
						NULL, NULL);
}

static void
pidgin_dialogs_alias_buddy_cb(PurpleBuddy *buddy, const char *new_alias)
{
	purple_buddy_set_local_alias(buddy, new_alias);
	purple_serv_alias_buddy(buddy);
}

void
pidgin_dialogs_alias_buddy(PurpleBuddy *buddy)
{
	gchar *secondary;

	g_return_if_fail(buddy != NULL);

	secondary = g_strdup_printf(_("Enter an alias for %s."), purple_buddy_get_name(buddy));

	purple_request_input(NULL, _("Alias Buddy"), NULL,
					   secondary, purple_buddy_get_local_alias(buddy), FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(pidgin_dialogs_alias_buddy_cb),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_account(purple_buddy_get_account(buddy)),
					   buddy);

	g_free(secondary);
}

static void
pidgin_dialogs_alias_chat_cb(PurpleChat *chat, const char *new_alias)
{
	purple_chat_set_alias(chat, new_alias);
}

void
pidgin_dialogs_alias_chat(PurpleChat *chat)
{
	gchar *alias;

	g_return_if_fail(chat != NULL);

	g_object_get(chat, "alias", &alias, NULL);

	purple_request_input(NULL, _("Alias Chat"), NULL,
					   _("Enter an alias for this chat."),
					   alias, FALSE, FALSE, NULL,
					   _("Alias"), G_CALLBACK(pidgin_dialogs_alias_chat_cb),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_account(purple_chat_get_account(chat)),
					   chat);

	g_free(alias);
}

static void
pidgin_dialogs_remove_contact_cb(PurpleContact *contact)
{
	PurpleBlistNode *bnode, *cnode;
	PurpleGroup *group;

	cnode = (PurpleBlistNode *)contact;
	group = (PurpleGroup*)cnode->parent;
	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		PurpleBuddy *buddy = (PurpleBuddy*)bnode;
		if (purple_account_is_connected(purple_buddy_get_account(buddy)))
			purple_account_remove_buddy(purple_buddy_get_account(buddy), buddy, group);
	}
	purple_blist_remove_contact(contact);
}

void
pidgin_dialogs_remove_contact(PurpleContact *contact)
{
	PurpleBuddy *buddy = purple_contact_get_priority_buddy(contact);

	g_return_if_fail(contact != NULL);
	g_return_if_fail(buddy != NULL);

	if (PURPLE_BLIST_NODE(contact)->child == PURPLE_BLIST_NODE(buddy) &&
	    PURPLE_BLIST_NODE(buddy)->next == NULL) {
		pidgin_dialogs_remove_buddy(buddy);
	} else {
		gchar *text;
		int contact_size = purple_counting_node_get_total_size(PURPLE_COUNTING_NODE(contact));
		text = g_strdup_printf(
					ngettext(
						"You are about to remove the contact containing %s "
						"and %d other buddy from your buddy list.  Do you "
						"want to continue?",
						"You are about to remove the contact containing %s "
						"and %d other buddies from your buddy list.  Do you "
						"want to continue?", contact_size - 1),
					purple_buddy_get_name(buddy), contact_size - 1);

		purple_request_action(contact, NULL, _("Remove Contact"), text, 0,
				NULL,
				contact, 2,
				_("_Remove Contact"), G_CALLBACK(pidgin_dialogs_remove_contact_cb),
				_("Cancel"),
				NULL);

		g_free(text);
	}
}

static void free_ggmo(struct _PidginGroupMergeObject *ggp)
{
	g_free(ggp->new_name);
	g_free(ggp);
}

static void
pidgin_dialogs_merge_groups_cb(struct _PidginGroupMergeObject *GGP)
{
	purple_group_set_name(GGP->parent, GGP->new_name);
	free_ggmo(GGP);
}

void
pidgin_dialogs_merge_groups(PurpleGroup *source, const char *new_name)
{
	gchar *text;
	struct _PidginGroupMergeObject *ggp;

	g_return_if_fail(source != NULL);
	g_return_if_fail(new_name != NULL);

	text = g_strdup_printf(
				_("You are about to merge the group called %s into the group "
				"called %s. Do you want to continue?"), purple_group_get_name(source), new_name);

	ggp = g_new(struct _PidginGroupMergeObject, 1);
	ggp->parent = source;
	ggp->new_name = g_strdup(new_name);

	purple_request_action(source, NULL, _("Merge Groups"), text, 0,
			NULL,
			ggp, 2,
			_("_Merge Groups"), G_CALLBACK(pidgin_dialogs_merge_groups_cb),
			_("Cancel"), G_CALLBACK(free_ggmo));

	g_free(text);
}

static void
pidgin_dialogs_remove_group_cb(PurpleGroup *group)
{
	PurpleBlistNode *cnode, *bnode;

	cnode = ((PurpleBlistNode*)group)->child;

	while (cnode) {
		if (PURPLE_IS_CONTACT(cnode)) {
			bnode = cnode->child;
			cnode = cnode->next;
			while (bnode) {
				PurpleBuddy *buddy;
				if (PURPLE_IS_BUDDY(bnode)) {
					buddy = (PurpleBuddy*)bnode;
					bnode = bnode->next;
					if (purple_account_is_connected(purple_buddy_get_account(buddy))) {
						purple_account_remove_buddy(purple_buddy_get_account(buddy), buddy, group);
						purple_blist_remove_buddy(buddy);
					}
				} else {
					bnode = bnode->next;
				}
			}
		} else if (PURPLE_IS_CHAT(cnode)) {
			PurpleChat *chat = (PurpleChat *)cnode;
			cnode = cnode->next;
			if (purple_account_is_connected(purple_chat_get_account(chat)))
				purple_blist_remove_chat(chat);
		} else {
			cnode = cnode->next;
		}
	}

	purple_blist_remove_group(group);
}

void
pidgin_dialogs_remove_group(PurpleGroup *group)
{
	gchar *text;

	g_return_if_fail(group != NULL);

	text = g_strdup_printf(_("You are about to remove the group %s and all its members from your buddy list.  Do you want to continue?"),
						   purple_group_get_name(group));

	purple_request_action(group, NULL, _("Remove Group"), text, 0,
						NULL,
						group, 2,
						_("_Remove Group"), G_CALLBACK(pidgin_dialogs_remove_group_cb),
						_("Cancel"), NULL);

	g_free(text);
}

/* XXX - Some of this should be moved into the core, methinks. */
static void
pidgin_dialogs_remove_buddy_cb(PurpleBuddy *buddy)
{
	PurpleGroup *group;
	gchar *name;
	PurpleAccount *account;

	group = purple_buddy_get_group(buddy);
	name = g_strdup(purple_buddy_get_name(buddy)); /* purple_buddy_get_name() is a crasher after remove_buddy */
	account = purple_buddy_get_account(buddy);

	purple_debug_info("blist", "Removing '%s' from buddy list.\n", purple_buddy_get_name(buddy));
	/* TODO - Should remove from blist first... then call purple_account_remove_buddy()? */
	purple_account_remove_buddy(account, buddy, group);
	purple_blist_remove_buddy(buddy);

	g_free(name);
}

void
pidgin_dialogs_remove_buddy(PurpleBuddy *buddy)
{
	gchar *text;

	g_return_if_fail(buddy != NULL);

	text = g_strdup_printf(_("You are about to remove %s from your buddy list.  Do you want to continue?"),
						   purple_buddy_get_name(buddy));

	purple_request_action(buddy, NULL, _("Remove Buddy"), text, 0,
		purple_request_cpar_from_account(
			purple_buddy_get_account(buddy)),
		buddy, 2, _("_Remove Buddy"),
		G_CALLBACK(pidgin_dialogs_remove_buddy_cb), _("Cancel"), NULL);

	g_free(text);
}

static void
pidgin_dialogs_remove_chat_cb(PurpleChat *chat)
{
	purple_blist_remove_chat(chat);
}

void
pidgin_dialogs_remove_chat(PurpleChat *chat)
{
	const gchar *name;
	gchar *text;

	g_return_if_fail(chat != NULL);

	name = purple_chat_get_name(chat);
	text = g_strdup_printf(_("You are about to remove the chat %s from your buddy list.  Do you want to continue?"),
			name ? name : "");

	purple_request_action(chat, NULL, _("Remove Chat"), text, 0,
		purple_request_cpar_from_account(purple_chat_get_account(chat)),
		chat, 2, _("_Remove Chat"),
		G_CALLBACK(pidgin_dialogs_remove_chat_cb), _("Cancel"), NULL);

	g_free(text);
}
