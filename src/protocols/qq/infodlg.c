/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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

// START OF FILE
/*****************************************************************************/
#include "debug.h"		// gaim_debug
#include "internal.h"		// strlen, _("get_text")
#include "notify.h"		// gaim_notify_error

#include "utils.h"		// get_face_gdkpixbuf
#include "char_conv.h"		// qq_to_utf8
#include "show.h"		// qq_show_get_image
#include "infodlg.h"

// must be defined after qq_infodlg.h to have <gtk/gtk.h>
#include "gtkdialogs.h"		// GAIM_DIALOG, mainwindow


// the follwoing definition is in UTF-8
const gint blood_types_count = 5;
const gchar *blood_types[] = {
	"其它", "A", "B", "O", "AB",
};

const gint country_names_count = 6;
const gchar *country_names[] = {
	"中国", "中国香港", "中国澳门", "中国台湾",
	"新加坡", "马来西亚", "美国",
};

const gint province_names_count = 34;
const gchar *province_names[] = {
	"北京", "天津", "上海", "重庆", "香港",
	"河北", "山西", "内蒙古", "辽宁", "吉林",
	"黑龙江", "江西", "浙江", "江苏", "安徽",
	"福建", "山东", "河南", "湖北", "湖南",
	"广东", "广西", "海南", "四川", "贵州",
	"云南", "西藏", "陕西", "甘肃", "宁夏",
	"青海", "新疆", "台湾", "澳门",
};

const gint zodiac_names_count = 13;
const gchar *zodiac_names[] = {
	"-", "鼠", "牛", "虎", "兔",
	"龙", "蛇", "马", "羊", "猴",
	"鸡", "狗", "猪",
};

const gint horoscope_names_count = 13;
const gchar *horoscope_names[] = {
	"-", "水瓶座", "双鱼座", "牡羊座", "金牛座",
	"双子座", "巨蟹座", "狮子座", "处女座", "天秤座",
	"天蝎座", "射手座", "魔羯座",
};

const gint occupation_names_count = 15;
const gchar *occupation_names[] = {
	"全职", "兼职", "制造业", "商业", "失业中",
	"学生", "工程师", "政府部门", "教育业", "服务行业",
	"老板", "计算机业", "退休", "金融业",
	"销售/广告/市场",
};

enum {
	QQ_CONTACT_OPEN = 0x00,
	QQ_CONTACT_ONLY_FRIENDS = 0x01,
	QQ_CONTACT_CLOSE = 0x02,
};


enum {
	QQ_AUTH_NO_AUTH = 0x00,
	QQ_AUTH_NEED_AUTH = 0x01,
	QQ_AUTH_NO_ADD = 0x02,
};

typedef struct _change_icon_widgets change_icon_widgets;

struct _change_icon_widgets {
	GtkWidget *dialog;	// dialog that shows all icons
	GtkWidget *face;	// the image widget we are going to change
};

/*****************************************************************************/
static void _window_deleteevent(GtkWidget * widget, GdkEvent * event, gpointer data) {
	gtk_widget_destroy(widget);	// this will call _window_destroy
}				// _window_deleteevent

/*****************************************************************************/
static void _window_close(GtkWidget * widget, gpointer data)
{
	// this will call _info_window_destroy if it is info-window
	gtk_widget_destroy(GTK_WIDGET(data));
}				// _window_close

/*****************************************************************************/
static void _no_edit(GtkWidget * w)
{
	gtk_editable_set_editable(GTK_EDITABLE(w), FALSE);
}				// _no_edit

/*****************************************************************************/
static GtkWidget *_qq_entry_new()
{
	GtkWidget *entry;
	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 255);	// set the max length
	return entry;
}				// _qq_entry_new

/*****************************************************************************/
static void _qq_set_entry(GtkWidget * entry, gchar * text)
{
	gchar *text_utf8;

	text_utf8 = qq_to_utf8(text, QQ_CHARSET_DEFAULT);
	gtk_entry_set_text(GTK_ENTRY(entry), text_utf8);

	g_free(text_utf8);
}				// _qq_set_entry

/*****************************************************************************/
static gchar *_qq_get_entry(GtkWidget * entry)
{
	return utf8_to_qq(gtk_entry_get_text(GTK_ENTRY(entry)), QQ_CHARSET_DEFAULT);
}				// _qq_get_entry

/*****************************************************************************/
static void _qq_set_text(GtkWidget * entry, gchar * text)
{
	gchar *text_utf8;

	text_utf8 = qq_to_utf8(text, QQ_CHARSET_DEFAULT);
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry)), text_utf8, -1);

	g_free(text_utf8);
}				// _qq_set_text

/*****************************************************************************/
static gchar *_qq_get_text(GtkWidget * entry)
{
	gchar *str, *ret;
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry)), &start, &end);
	str = gtk_text_buffer_get_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry)), &start, &end, FALSE);
	ret = utf8_to_qq(str, QQ_CHARSET_DEFAULT);

	return ret;
}				// _qq_get_text

/*****************************************************************************/
static void _qq_set_image(GtkWidget * entry, gint index)
{
	GdkPixbuf *pixbuf;

	g_return_if_fail(entry != NULL && index >= 0);

	pixbuf = get_face_gdkpixbuf(index);
	gtk_image_set_from_pixbuf(GTK_IMAGE(entry), pixbuf);
	g_object_unref(pixbuf);
	g_object_set_data(G_OBJECT(entry), "user_data", GINT_TO_POINTER(index));
}				// _qq_set_image


/*****************************************************************************/
static void _qq_change_face(GtkWidget * w, gpointer * user_data)
{
	gint index;
	change_icon_widgets *change_icon;

	change_icon = (change_icon_widgets *) user_data;

	index = (gint) g_object_get_data(G_OBJECT(w), "user_data");

	_qq_set_image(change_icon->face, index);
	_window_close(NULL, change_icon->dialog);

	g_free(change_icon);

}				// _qq_change_face

/*****************************************************************************/
static GList *_get_list_by_array(gchar ** array, gint size)
{
	gint i;
	GList *cbitems;

	cbitems = NULL;
	for (i = 0; i < size; i++)
		cbitems = g_list_append(cbitems, array[i]);
	return cbitems;
}				// _get_list_by_array

/*****************************************************************************/
static void _qq_set_open_contact_radio(contact_info_window * info_window, gchar * is_open_contact) {
	gint open;
	GtkWidget **radio;

	open = atoi(is_open_contact);
	radio = info_window->open_contact_radio;

	if (open == QQ_CONTACT_OPEN)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[0]), TRUE);
	else if (open == QQ_CONTACT_ONLY_FRIENDS)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[1]), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[2]), TRUE);
}				// _qq_set_open_contact_radio

/*****************************************************************************/
static void _qq_set_auth_type_radio(contact_info_window * info_window, gchar * auth_type_str) {
	gint auth_type;
	GtkWidget **radio;

	auth_type = atoi(auth_type_str);
	radio = info_window->auth_radio;

	if (auth_type == QQ_AUTH_NO_AUTH)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[0]), TRUE);
	else if (auth_type == QQ_AUTH_NEED_AUTH)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[1]), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[2]), TRUE);
}				// _qq_set_auth_type_radio

/*****************************************************************************/
static void _info_window_destroy(GtkWidget * widget, gpointer data)
{
	GList *list;
	qq_data *qd;
	GaimConnection *gc;
	contact_info_window *info_window;

	gc = (GaimConnection *) data;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	if (QQ_DEBUG)
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Destroy info window.\n");
	qd = (qq_data *) gc->proto_data;
	list = qd->contact_info_window;

	while (list) {
		info_window = (contact_info_window *) (list->data);
		if (info_window->window != widget)
			list = list->next;
		else {
			if (info_window->old_info)
				g_strfreev((gchar **) info_window->old_info);
			qd->contact_info_window = g_list_remove(qd->contact_info_window, info_window);
			g_free(info_window);
			break;
		}
	}			// while
}				// _info_window_destory

/*****************************************************************************/
void qq_contact_info_window_free(qq_data * qd)
{
	gint i;
	contact_info_window *info;

	g_return_if_fail(qd != NULL);

	i = 0;
	while (qd->contact_info_window) {
		info = (contact_info_window *) qd->contact_info_window->data;
		// remove the entry before free it
		// in some rare cases, system might automatically (stupidly) 
		// remove the entry from the list when user free the entry, 
		// then, qd->contact_info_window becomes NULL and it causes crash
		qd->contact_info_window = g_list_remove(qd->contact_info_window, info);
		if (info->window)
			gtk_widget_destroy(info->window);
		g_free(info);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d conatct_info_window are freed\n", i);
}				// qq_contact_info_window_free

/*****************************************************************************/
static void _info_window_refresh(GtkWidget * widget, gpointer data)
{
	GaimConnection *gc;
	contact_info_window *info_window;

	gc = (GaimConnection *) data;
	info_window = (contact_info_window *)
	    g_object_get_data(G_OBJECT(widget), "user_data");

	qq_send_packet_get_info(gc, info_window->uid, TRUE);
	gtk_widget_set_sensitive(info_window->refresh_button, FALSE);
}				// _info_window_refresh

/*****************************************************************************/
static void _info_window_change(GtkWidget * widget, gpointer data)
{
	gchar *passwd[3];
	gboolean is_modify_passwd;
	guint8 icon, auth_type, open_contact;
	qq_data *qd;
	GaimAccount *a;
	GaimConnection *gc;
	contact_info *info;
	contact_info_window *info_window;

	gc = (GaimConnection *) data;
	a = gc->account;
	qd = gc->proto_data;
	info_window = (contact_info_window *)
	    g_object_get_data(G_OBJECT(widget), "user_data");
	info = info_window->old_info;

	is_modify_passwd = FALSE;
	passwd[0] = passwd[1] = passwd[2] = NULL;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->is_modify_passwd)))
		is_modify_passwd = TRUE;

	if (is_modify_passwd) {
		passwd[0] = _qq_get_entry(info_window->old_password);
		passwd[1] = _qq_get_entry(info_window->password[0]);
		passwd[2] = _qq_get_entry(info_window->password[1]);

		if (g_ascii_strcasecmp(a->password, passwd[0])) {
			gaim_notify_error(gc, _("Error"), _("Old password is wrong"), NULL);
			g_free(passwd[0]);
			g_free(passwd[1]);
			g_free(passwd[2]);
			return;
		}
		if (passwd[1][0] == '\0') {
			gaim_notify_error(gc, _("Error"), _("Password can not be empty"), NULL);
			g_free(passwd[0]);
			g_free(passwd[1]);
			g_free(passwd[2]);
			return;
		}
		if (g_ascii_strcasecmp(passwd[1], passwd[2])) {
			gaim_notify_error(gc, _("Error"),
					  _("Confirmed password is not the same as new password"), NULL);
			g_free(passwd[0]);
			g_free(passwd[1]);
			g_free(passwd[2]);
			return;
		}
		if (strlen(passwd[1]) > 16) {
			gaim_notify_error(gc, _("Error"), _("Password can not longer than 16 characters"), NULL);
			g_free(passwd[0]);
			g_free(passwd[1]);
			g_free(passwd[2]);
			return;
		}
	}			// if (is_modify_passwd)

	icon = (gint) g_object_get_data(G_OBJECT(info_window->face), "user_data");

	info->face = g_strdup_printf("%d", icon);
	info->nick = _qq_get_entry(info_window->nick);
	info->age = _qq_get_entry(info_window->age);
	info->gender = _qq_get_entry(info_window->gender);
	info->country = _qq_get_entry(info_window->country);
	info->province = _qq_get_entry(info_window->province);
	info->city = _qq_get_entry(info_window->city);
	info->email = _qq_get_entry(info_window->email);
	info->address = _qq_get_entry(info_window->address);
	info->zipcode = _qq_get_entry(info_window->zipcode);
	info->tel = _qq_get_entry(info_window->tel);
	info->name = _qq_get_entry(info_window->name);
	info->college = _qq_get_entry(info_window->college);
	info->occupation = _qq_get_entry(info_window->occupation);
	info->homepage = _qq_get_entry(info_window->homepage);
	info->intro = _qq_get_text(info_window->intro);

	info->blood =
	    get_index_str_by_name((gchar **) blood_types,
				  gtk_entry_get_text(GTK_ENTRY(info_window->blood)), blood_types_count);
	info->zodiac =
	    get_index_str_by_name((gchar **) zodiac_names,
				  gtk_entry_get_text(GTK_ENTRY(info_window->zodiac)), zodiac_names_count);
	info->horoscope =
	    get_index_str_by_name((gchar **) horoscope_names,
				  gtk_entry_get_text(GTK_ENTRY(info_window->horoscope)), horoscope_names_count);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->auth_radio[0])))
		auth_type = QQ_AUTH_NO_AUTH;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->auth_radio[1])))
		auth_type = QQ_AUTH_NEED_AUTH;
	else
		auth_type = QQ_AUTH_NO_ADD;

	info->auth_type = g_strdup_printf("%d", auth_type);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->open_contact_radio[0])))
		open_contact = QQ_CONTACT_OPEN;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(info_window->open_contact_radio[1])))
		open_contact = QQ_CONTACT_ONLY_FRIENDS;
	else
		open_contact = QQ_CONTACT_CLOSE;

	info->is_open_contact = g_strdup_printf("%d", open_contact);

	qq_send_packet_modify_info(gc, info, passwd[1]);
	qq_refresh_buddy_and_myself(info, gc);

	gtk_widget_destroy(info_window->window);	//close window
}

/*****************************************************************************/
static void _info_window_change_face(GtkWidget * widget, GdkEvent * event, contact_info_window * info_window) {
	gint i;
	GdkPixbuf *pixbuf;
	GtkWidget *dialog, *image, *button, *vbox, *smiley_box;
	change_icon_widgets *change_icon;

	change_icon = g_new0(change_icon_widgets, 1);

	smiley_box = NULL;
	GAIM_DIALOG(dialog);	// learned from gtkimhtmltoolbar.c
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);

	g_signal_connect(G_OBJECT(dialog), "delete_event", G_CALLBACK(_window_deleteevent), NULL);

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

	change_icon->dialog = dialog;
	change_icon->face = info_window->face;

	vbox = gtk_vbox_new(TRUE, 5);

	for (i = 0; i < 85; i++) {
		if (i % 8 == 0) {
			smiley_box = gtk_toolbar_new();
			gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);
		}
		pixbuf = get_face_gdkpixbuf(i * 3);
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(pixbuf);
		button = gtk_toolbar_append_item(GTK_TOOLBAR(smiley_box),
						 NULL, NULL, NULL, image, G_CALLBACK(_qq_change_face), change_icon);
		g_object_set_data(G_OBJECT(button), "user_data", GINT_TO_POINTER(i * 3));
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	}			// for i

	gtk_container_add(GTK_CONTAINER(dialog), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Choose my head icon"));
	gtk_widget_show_all(dialog);
}				// _info_window_change_face

/*****************************************************************************/
static void _change_passwd_checkbutton_callback(GtkWidget * widget, contact_info_window * info_window) {
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		gtk_widget_set_sensitive(info_window->old_password, TRUE);
		gtk_widget_set_sensitive(info_window->password[0], TRUE);
		gtk_widget_set_sensitive(info_window->password[1], TRUE);
	} else {
		gtk_widget_set_sensitive(info_window->old_password, FALSE);
		gtk_widget_set_sensitive(info_window->password[0], FALSE);
		gtk_widget_set_sensitive(info_window->password[1], FALSE);
	}
}				// _change_passwd_checkbutton_callback

/*****************************************************************************/
static GtkWidget *_create_page_basic
    (gint is_myself, contact_info * info, GaimConnection * gc, contact_info_window * info_window) {
	GdkPixbuf *pixbuf;
	GList *cbitems;
	GtkWidget *hbox, *pixmap, *frame, *label, *table;
	GtkWidget *entry, *combo, *alignment;
	GtkWidget *event_box, *qq_show;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	cbitems = NULL;

	hbox = gtk_hbox_new(FALSE, 0);
	frame = gtk_frame_new("");
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

	table = gtk_table_new(7, 3, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	pixbuf = get_face_gdkpixbuf((guint8) strtol(info->face, NULL, 10));
	pixmap = gtk_image_new_from_pixbuf(pixbuf);
	info_window->face = pixmap;

	alignment = gtk_alignment_new(0.25, 0, 0, 0);

	// set up icon
	if (is_myself) {
		g_object_set_data(G_OBJECT(info_window->face), "user_data",
				  GINT_TO_POINTER(strtol(info->face, NULL, 10)));
		event_box = gtk_event_box_new();
		g_signal_connect(G_OBJECT(event_box), "button_press_event",
				 G_CALLBACK(_info_window_change_face), info_window);
		gtk_container_add(GTK_CONTAINER(event_box), pixmap);
		gtk_container_add(GTK_CONTAINER(alignment), event_box);
		gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), event_box, _("Click to change icon"), NULL);
	} else
		gtk_container_add(GTK_CONTAINER(alignment), pixmap);

	gtk_table_attach(GTK_TABLE(table), alignment, 2, 3, 0, 2, GTK_FILL, 0, 0, 0);

	// set up qq_show image and event
	qq_show = qq_show_default(info);
	g_object_set_data(G_OBJECT(qq_show), "user_data", GINT_TO_POINTER((gint) atoi(info->uid)));

	frame = gtk_frame_new("");
	gtk_container_set_border_width(GTK_CONTAINER(frame), 0);

	if (strtol(info->qq_show, NULL, 10) != 0) {	// buddy has qq_show
		event_box = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(frame), event_box);
		gtk_container_add(GTK_CONTAINER(event_box), qq_show);
		g_signal_connect(G_OBJECT(event_box), "button_press_event", G_CALLBACK(qq_show_get_image), qq_show);
		gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), event_box, _("Click to refresh"), NULL);
	} else			// no event_box
		gtk_container_add(GTK_CONTAINER(frame), qq_show);

	gtk_table_attach(GTK_TABLE(table), frame, 2, 3, 2, 7, GTK_EXPAND, 0, 0, 0);

	label = gtk_label_new(_("QQid"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Nickname"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->uid_entry = entry;
	_qq_set_entry(info_window->uid_entry, info->uid);
	gtk_widget_set_size_request(entry, 120, -1);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->nick = entry;
	_qq_set_entry(info_window->nick, info->nick);
	gtk_widget_set_size_request(entry, 120, -1);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Age"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Gender"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Country/Region"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);

	combo = gtk_combo_new();
	cbitems = _get_list_by_array((gchar **) country_names, country_names_count);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->country = GTK_COMBO(combo)->entry;
	_qq_set_entry(info_window->country, info->country);
	gtk_widget_set_size_request(combo, 150, -1);
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	entry = _qq_entry_new();
	info_window->age = entry;
	_qq_set_entry(info_window->age, info->age);
	gtk_widget_set_size_request(entry, 40, -1);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	combo = gtk_combo_new();
	cbitems = g_list_append(cbitems, (char *)_("Male"));
	cbitems = g_list_append(cbitems, (char *)_("Female"));
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->gender = GTK_COMBO(combo)->entry;
	gtk_widget_set_size_request(combo, 60, -1);
	_qq_set_entry(info_window->gender, info->gender);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	label = gtk_label_new(_("Province"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("City"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7, GTK_FILL, 0, 0, 0);

	combo = gtk_combo_new();
	cbitems = _get_list_by_array((gchar **) province_names, province_names_count);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->province = GTK_COMBO(combo)->entry;
	_qq_set_entry(info_window->province, info->province);
	gtk_widget_set_size_request(combo, 150, -1);
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	entry = _qq_entry_new();
	info_window->city = entry;
	gtk_widget_set_size_request(entry, 120, -1);
	_qq_set_entry(info_window->city, info->city);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 6, 7, GTK_FILL, 0, 0, 0);

	return hbox;
}				// _create_page_basic

/*****************************************************************************/
static GtkWidget *_create_page_contact
    (int is_myself, contact_info * info, GaimConnection * gc, contact_info_window * info_window) {
	GtkWidget *vbox1, *frame;
	GtkWidget *hbox1, *entry, *label;
	GtkWidget *hbox, *radio, *table;

	hbox = gtk_hbox_new(FALSE, 0);
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 0);

	frame = gtk_frame_new("");
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, TRUE, TRUE, 0);

	table = gtk_table_new(2, 5, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	label = gtk_label_new(_("Email"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Address"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Zipcode"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Tel"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Homepage"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->email = entry;
	gtk_widget_set_size_request(entry, 240, -1);
	_qq_set_entry(info_window->email, info->email);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->address = entry;
	_qq_set_entry(info_window->address, info->address);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->zipcode = entry;
	_qq_set_entry(info_window->zipcode, info->zipcode);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->tel = entry;
	_qq_set_entry(info_window->tel, info->tel);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->homepage = entry;
	_qq_set_entry(info_window->homepage, info->homepage);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);

	frame = gtk_frame_new(_("Above infomation"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	if (!is_myself)
		gtk_widget_set_sensitive(frame, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	radio = gtk_radio_button_new_with_label(NULL, _("Open to all"));
	info_window->open_contact_radio[0] = radio;
	gtk_box_pack_start(GTK_BOX(hbox1), radio, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget
	    (GTK_RADIO_BUTTON(radio), _("Only accessible by my frineds"));
	info_window->open_contact_radio[1] = radio;
	gtk_box_pack_start(GTK_BOX(hbox1), radio, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), _("Close to all"));
	info_window->open_contact_radio[2] = radio;
	gtk_box_pack_start(GTK_BOX(hbox1), radio, FALSE, FALSE, 0);

	_qq_set_open_contact_radio(info_window, info->is_open_contact);

	return hbox;
}				// _create_page_contact

/*****************************************************************************/
static GtkWidget *_create_page_details
    (int is_myself, contact_info * info, GaimConnection * gc, contact_info_window * info_window) {
	GList *cbitems;
	GtkWidget *text, *hbox, *frame;
	GtkWidget *combo, *table, *label, *entry, *scrolled_window;

	cbitems = NULL;

	hbox = gtk_hbox_new(FALSE, 0);
	frame = gtk_frame_new("");
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

	table = gtk_table_new(4, 4, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	label = gtk_label_new(_("Real Name"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_(" Zodiac"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_(" Blood Type"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_(" Horoscope"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 2, 3, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("College"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Career"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Intro"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->name = entry;
	gtk_widget_set_size_request(entry, 120, -1);
	_qq_set_entry(info_window->name, info->name);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	info_window->college = entry;
	gtk_widget_set_size_request(entry, 120, -1);
	_qq_set_entry(info_window->college, info->college);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	combo = gtk_combo_new();
	cbitems = _get_list_by_array((gchar **) zodiac_names, zodiac_names_count);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->zodiac = GTK_COMBO(combo)->entry;
	gtk_entry_set_text(GTK_ENTRY(info_window->zodiac),
			   get_name_by_index_str((gchar **) zodiac_names, info->zodiac, zodiac_names_count));
	gtk_widget_set_size_request(combo, 70, -1);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), combo, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	combo = gtk_combo_new();
	cbitems = _get_list_by_array((gchar **) blood_types, blood_types_count);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->blood = GTK_COMBO(combo)->entry;
	gtk_entry_set_text(GTK_ENTRY(info_window->blood),
			   get_name_by_index_str((gchar **) blood_types, info->blood, blood_types_count));
	gtk_widget_set_size_request(combo, 70, -1);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), combo, 3, 4, 1, 2, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	combo = gtk_combo_new();
	cbitems = _get_list_by_array((gchar **) horoscope_names, horoscope_names_count);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->horoscope = GTK_COMBO(combo)->entry;
	gtk_entry_set_text(GTK_ENTRY(info_window->horoscope), get_name_by_index_str((gchar **)
										    horoscope_names,
										    info->horoscope,
										    horoscope_names_count));
	gtk_widget_set_size_request(combo, 70, -1);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), combo, 3, 4, 2, 3, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	combo = gtk_combo_new();
	cbitems = _get_list_by_array((gchar **) occupation_names, occupation_names_count);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cbitems);
	g_list_free(cbitems);
	cbitems = NULL;
	info_window->occupation = GTK_COMBO(combo)->entry;
	_qq_set_entry(info_window->occupation, info->occupation);
	gtk_widget_set_size_request(combo, 120, -1);
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
	if (!is_myself)
		gtk_widget_set_sensitive(combo, FALSE);

	text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	info_window->intro = text;
	gtk_widget_set_size_request(text, -1, 90);
	_qq_set_text(info_window->intro, info->intro);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), text);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 2);

	gtk_table_attach(GTK_TABLE(table), scrolled_window, 1, 4, 3, 4, GTK_FILL, 0, 0, 0);
	return hbox;
}				// _create_page_details

/*****************************************************************************/
static GtkWidget *_create_page_security
    (int is_myself, contact_info * info, GaimConnection * gc, contact_info_window * info_window) {
	GtkWidget *hbox, *frame, *vbox1, *vbox2;
	GtkWidget *entry, *table, *label, *check_button;
	GtkWidget *radio, *alignment;

	hbox = gtk_hbox_new(FALSE, 0);
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 0);

	frame = gtk_frame_new(_("Change Password"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	table = gtk_table_new(3, 3, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	label = gtk_label_new(_("Old Passwd"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	check_button = gtk_check_button_new_with_label(_("I wanna change"));
	info_window->is_modify_passwd = check_button;
	g_signal_connect(G_OBJECT(check_button), "toggled",
			 G_CALLBACK(_change_passwd_checkbutton_callback), info_window);
	gtk_table_attach(GTK_TABLE(table), check_button, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("New Passwd"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("(less than 16 char)"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Confirm"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_widget_set_sensitive(entry, FALSE);
	info_window->old_password = entry;
	gtk_widget_set_size_request(entry, 160, -1);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_widget_set_sensitive(entry, FALSE);
	info_window->password[0] = entry;
	gtk_widget_set_size_request(entry, 160, -1);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

	entry = _qq_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_widget_set_sensitive(entry, FALSE);
	info_window->password[1] = entry;
	gtk_widget_set_size_request(entry, 160, -1);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

	alignment = gtk_alignment_new(0, 0, 0, 0);
	gtk_widget_set_size_request(alignment, -1, 5);
	gtk_box_pack_start(GTK_BOX(vbox1), alignment, FALSE, FALSE, 0);

	frame = gtk_frame_new(_("Authentication"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	radio = gtk_radio_button_new_with_label(NULL, _("Anyone can add me"));
	info_window->auth_radio[0] = radio;
	gtk_box_pack_start(GTK_BOX(vbox2), radio, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget
	    (GTK_RADIO_BUTTON(radio), _("User needs to be authenticated to add me"));
	info_window->auth_radio[1] = radio;
	gtk_box_pack_start(GTK_BOX(vbox2), radio, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), _("Nobody can add me"));
	info_window->auth_radio[2] = radio;
	gtk_box_pack_start(GTK_BOX(vbox2), radio, FALSE, FALSE, 0);

	_qq_set_auth_type_radio(info_window, info->auth_type);

	return hbox;
}				// _create_page_security 

/*****************************************************************************/
void qq_show_contact_info_dialog(contact_info * info, GaimConnection * gc, contact_info_window * info_window) {

	GaimAccount *a = gc->account;
	gboolean is_myself;
	GtkWidget *label, *window, *notebook, *vbox, *bbox, *button;

	is_myself = (!strcmp(info->uid, a->username));
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
//	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(mainwindow));  //by gfhuang
	info_window->window = window;

	// we need to duplicate here, 
	// as the passed-in info structure will be freed in his function
	info_window->old_info = (contact_info *) g_strdupv((gchar **) info);

	/* When the window is given the "delete_event" signal (this is given
	 * by the window manager, usually by the "close" option, or on the
	 * titlebar), we ask it to call the delete_event () function
	 * as defined above. The data passed to the callback  
	 * function is NULL and is ignored in the callback function. 
	 */
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(_window_deleteevent), NULL);

	/* Here we connect the "destroy" event to a signal handler.  
	 * This event occurs when we call gtk_widget_destroy() on the window,
	 * or if we return FALSE in the "delete_event" callback. 
	 */
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(_info_window_destroy), gc);

	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

	if (is_myself)
		gtk_window_set_title(GTK_WINDOW(window), _("My Information"));
	else
		gtk_window_set_title(GTK_WINDOW(window), _("My Buddy's Information"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);

	label = gtk_label_new(_("Basic"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), _create_page_basic(is_myself, info, gc, info_window), label);

	label = gtk_label_new(_("Contact"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), _create_page_contact(is_myself, info, gc, info_window), label);

	label = gtk_label_new(_("Details"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), _create_page_details(is_myself, info, gc, info_window), label);

	if (is_myself) {
		label = gtk_label_new(_("Security"));
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
					 _create_page_security(is_myself, info, gc, info_window), label);
	}

	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 10);

	button = gtk_button_new_with_label(_("Modify"));
	g_object_set_data(G_OBJECT(button), "user_data", (gpointer) info_window);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(_info_window_change), gc);
	gtk_container_add(GTK_CONTAINER(bbox), button);

	if (is_myself)
		gtk_widget_set_sensitive(button, TRUE);
	else
		gtk_widget_set_sensitive(button, FALSE);

	button = gtk_button_new_with_label(_("Refresh"));
	info_window->refresh_button = button;
	g_object_set_data(G_OBJECT(button), "user_data", (gpointer) info_window);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(_info_window_refresh), gc);
	gtk_container_add(GTK_CONTAINER(bbox), button);

	button = gtk_button_new_with_label(_("Close"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(_window_close), info_window->window);
	gtk_container_add(GTK_CONTAINER(bbox), button);

	gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);

	_no_edit(info_window->uid_entry);	// uid entry cannot be changed
	if (!is_myself) {
		_no_edit(info_window->nick);
		_no_edit(info_window->country);
		_no_edit(info_window->age);
		_no_edit(info_window->gender);
		_no_edit(info_window->province);
		_no_edit(info_window->city);
		_no_edit(info_window->email);
		_no_edit(info_window->address);
		_no_edit(info_window->zipcode);
		_no_edit(info_window->tel);
		_no_edit(info_window->name);
		_no_edit(info_window->blood);
		_no_edit(info_window->college);
		_no_edit(info_window->occupation);
		_no_edit(info_window->zodiac);
		_no_edit(info_window->horoscope);
		_no_edit(info_window->homepage);
		gtk_text_view_set_editable(GTK_TEXT_VIEW(info_window->intro), FALSE);
	}
	gtk_widget_show_all(window);
}				// qq_show_contact_info_dialog

/*****************************************************************************/
void qq_refresh_contact_info_dialog(contact_info * new_info, GaimConnection * gc, contact_info_window * info_window) {
	GaimAccount *a;
	gboolean is_myself;
	contact_info *info;
	a = gc->account;

	if (info_window->old_info)
		g_strfreev((gchar **) info_window->old_info);

	// we need to duplicate here, 
	// as the passed-in info structure will be freed in his function
	info = (contact_info *) g_strdupv((gchar **) new_info);
	info_window->old_info = info;

	is_myself = !(g_ascii_strcasecmp(info->uid, a->username));
	gtk_widget_set_sensitive(info_window->refresh_button, TRUE);

	if (is_myself) {
		_qq_set_auth_type_radio(info_window, info->auth_type);
		_qq_set_open_contact_radio(info_window, info->is_open_contact);
	}

	_qq_set_entry(info_window->uid_entry, info->uid);
	_qq_set_entry(info_window->nick, info->nick);
	_qq_set_entry(info_window->country, info->country);
	_qq_set_entry(info_window->age, info->age);
	_qq_set_entry(info_window->gender, info->gender);
	_qq_set_entry(info_window->province, info->province);
	_qq_set_entry(info_window->city, info->city);
	_qq_set_entry(info_window->email, info->email);
	_qq_set_entry(info_window->address, info->address);
	_qq_set_entry(info_window->zipcode, info->zipcode);
	_qq_set_entry(info_window->tel, info->tel);
	_qq_set_entry(info_window->name, info->name);
	gtk_entry_set_text(GTK_ENTRY(info_window->zodiac),
			   get_name_by_index_str((gchar **) zodiac_names, info->zodiac, zodiac_names_count));
	gtk_entry_set_text(GTK_ENTRY(info_window->horoscope), get_name_by_index_str((gchar **)
										    horoscope_names,
										    info->horoscope,
										    horoscope_names_count));
	gtk_entry_set_text(GTK_ENTRY(info_window->blood),
			   get_name_by_index_str((gchar **) blood_types, info->blood, blood_types_count));
	_qq_set_entry(info_window->college, info->college);
	_qq_set_entry(info_window->occupation, info->occupation);
	_qq_set_entry(info_window->homepage, info->homepage);

	_qq_set_image(info_window->face, (guint8) atoi(info->face));

	_qq_set_text(info_window->intro, info->intro);
}				// qq_refresh_contact_info_dialog

/*****************************************************************************/
// END OF FILE
