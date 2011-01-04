#include <stdio.h>
#include <assert.h>

#include "internal.h"
#include "plugin.h"
#include "gtkplugin.h"
#include "gtkblist.h"
#include "gtkutils.h"
#include "connection.h"
#include "conversation.h"
#include "network.h"

#include <gtk/gtkgl.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "crazychat.h"
#include "cc_network.h"
#include "cc_interface.h"
#include "cc_gtk_gl.h"
#include "util.h"

/* --- begin type and global variable definitions --- */

static struct crazychat cc_info;

/* --- begin function declarations --- */

/**
 * Called by purple plugin to start CrazyChat
 * @param cc	the crazychat struct
 */
static void cc_init(struct crazychat *cc);

/**
 * Called by purple plugin to destroy CrazyChat
 * @param cc	the crazychat struct
 */
static void cc_destroy(struct crazychat *cc);


/**
 * Buddy menu drawing callback.  Adds a CrazyChat menuitem.
 * @param menu	the buddy menu widget
 * @param b	the buddy whose menu this is
 */
static gboolean cc_buddy_menu(GtkWidget *menu, PurpleBuddy *b);

/**
 * Buddy menu callback.  Initiates the CrazyChat session.
 * @param item	the gtk buddy menu item
 * @param b	the buddy whose menu the item was in
 */
static void cc_menu_cb(GtkMenuItem *item, PurpleBuddy *b);

/**
 * IM callback.  Handles receiving a CrazyChat session request.
 * @param account	the account we received the IM on
 * @param sender	the buddy who we received the message from
 * @param message	the message we received
 * @param flags		IM flags
 * @param data		user data
 */
static gboolean receive_im_cb(PurpleAccount *account, char **sender,
		char **message,	int *flags, void *data);

/**
 * Displaying IM callback.  Drops CrazyChat messages from IM window.
 * @param account	the account we are displaying the IM on
 * @param conv		the conversation we are displaying the IM on
 * @param message	the message we are displaying
 * @param data		user data
 */
static gboolean display_im_cb(PurpleAccount *account, const char *who, char **message,
			PurpleConnection *conv, PurpleMessageFlags flags, void *data);

/**
 * Callback for CrazyChat plugin configuration frame
 * @param plugin	the plugin data
 * @return	the configuration frame
 */
static GtkWidget *get_config_frame(PurplePlugin *plugin);

/**
 * TCP port callback.  Changes the port used to listen for new CC sessions
 * @param spin		the spinner button whose value changed
 * @param data		user data
 */
static void tcp_port_cb(GtkSpinButton *spin, struct crazychat *cc);

/**
 * UDP port callback.  Changes the port used to send/recv CC session frames
 * @param spin		the spinner button whose value changed
 * @param data		user data
 */
static void udp_port_cb(GtkSpinButton *spin, struct crazychat *cc);

/**
 * Features enabling/disabling callback.  Initializes the input processing
 * or shuts it down.
 * @param data		user data
 */
static void features_enable_cb(struct crazychat *cc);

/**
 * User signed on callback.  Now we have a buddy list to connect a signal
 * handler to.
 * @param gc		the purple connection we are signed on
 * @param plugin	our plugin struct
 */
static gboolean cc_signed_on(PurpleConnection *gc, void *plugin);

/**
 * Plugin loading callback.  If a buddy list exists, connect our buddy menu
 * drawing callback to the signal handler, otherwise, connect a signed on
 * signal handler so we know when we get a buddy list.
 * @param plugin	our plugin struct
 */
static gboolean plugin_load(PurplePlugin *plugin);

/**
 * Plugin unloading callback.  Disconnect all handlers and free data.
 * @param plugin	our plugin struct
 */
static gboolean plugin_unload(PurplePlugin *plugin);


/* --- end function declarations --- */


#define CRAZYCHAT_PLUGIN_ID "gtk-crazychat"

static PidginPluginUiInfo ui_info = {
	get_config_frame				/**< get_config_frame */
};

static PurplePluginInfo info = {
	2,						  /**< api_version    */
	PURPLE_PLUGIN_STANDARD,				  /**< type           */
	PIDGIN_PLUGIN_TYPE,				  /**< ui_requirement */
	0,						  /**< flags          */
	NULL,						  /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,				  /**< priority       */

	CRAZYCHAT_PLUGIN_ID,				  /**< id             */
	N_("Crazychat"),				  /**< name           */
	DISPLAY_VERSION,				  /**< version        */
							  /**  summary        */
	N_("Plugin to establish a Crazychat session."),
							  /**  description    */
	N_("Uses Purple to obtain buddy ips to connect for a Crazychat session"),
	"\n"
	"William Chan <chanman@stanford.edu>\n"
	"Ian Spiro <ispiro@stanford.edu>\n"
	"Charlie Stockman<stockman@stanford.edu>\n"
	"Steve Yelderman<scy@stanford.edu>",		  /**< author         */
	PURPLE_WEBSITE,					  /**< homepage       */

	plugin_load,					  /**< load           */
	plugin_unload,					  /**< unload         */
	NULL,						  /**< destroy        */

	&ui_info,					  /**< ui_info        */
	&cc_info					  /**< extra_info     */
};

/* --- end plugin struct definition --- */

static void cc_init(struct crazychat *cc)
{
	/* initialize main crazychat thread */

	assert(cc);
	memset(cc, 0, sizeof(*cc));

	/* initialize network configuration */
	cc->tcp_port = DEFAULT_CC_PORT;
	cc->udp_port = DEFAULT_CC_PORT;

	/* disable input subsystem */
	//cc->features_state = 0;

	/* initialize input subsystem */
	cc->features_state = 1;
	cc->input_data = init_input(cc);
}

static void cc_destroy(struct crazychat *cc)
{
	assert(cc);

	if (cc->features_state) {
		destroy_input(cc->input_data);
	}
	memset(cc, 0, sizeof(*cc));
}

static gboolean cc_buddy_menu(GtkWidget *menu, PurpleBuddy *b)
{
	GtkWidget *menuitem;

	menuitem = gtk_menu_item_new_with_mnemonic("CrazyChat");
	g_signal_connect(G_OBJECT(menuitem), "activate",
			G_CALLBACK(cc_menu_cb), b);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	return FALSE;
}

static void cc_menu_cb(GtkMenuItem *item, PurpleBuddy *b)
{
	assert(item);
	assert(b);

	/* send the invite */
	cc_net_send_invite(&cc_info, b->name, b->account);
}

static gboolean receive_im_cb(PurpleAccount *account, char **sender,
		char **message,	int *flags, void *data)
{
	struct crazychat *cc;

	cc = (struct crazychat*)data;
	assert(cc);
	if (!strncmp(*message, CRAZYCHAT_INVITE_CODE,
				strlen(CRAZYCHAT_INVITE_CODE))) {
		Debug(*message);
		char *split = strchr(*message, '!');
		assert(split);
		*split = 0;
		split++;
		cc_net_recv_invite(account, cc, *sender,
				&(*message)[strlen(CRAZYCHAT_INVITE_CODE)],
				split);
		return TRUE;
	} else if (!strncmp(*message, CRAZYCHAT_ACCEPT_CODE,
				strlen(CRAZYCHAT_ACCEPT_CODE))) {
		cc_net_recv_accept(account, cc, *sender,
				&(*message)[strlen(CRAZYCHAT_ACCEPT_CODE)]);
		return TRUE;
	} else if (!strncmp(*message, CRAZYCHAT_READY_CODE,
				strlen(CRAZYCHAT_READY_CODE))) {
		cc_net_recv_ready(account, cc, *sender);
		return TRUE;
	}

	return FALSE;
}

static gboolean display_im_cb(PurpleAccount *account, PurpleConversation *conv,
		char **message, void *data)
{
	struct crazychat *cc;

	cc = (struct crazychat*)data;
	assert(cc);
	if (!strncmp(*message, CRAZYCHAT_INVITE_CODE,
				strlen(CRAZYCHAT_INVITE_CODE))) {
		return TRUE;
	} else if (!strncmp(*message, CRAZYCHAT_ACCEPT_CODE,
				strlen(CRAZYCHAT_ACCEPT_CODE))) {
		return TRUE;
	} else if (!strncmp(*message, CRAZYCHAT_READY_CODE,
				strlen(CRAZYCHAT_READY_CODE))) {
		return TRUE;
	}

	return FALSE;
}

static GtkWidget *get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox, *hbox;
	GtkWidget *drawing_area;
	GtkWidget *label;
	GtkAdjustment *adj;
	GtkWidget *spinner;
	GtkWidget *button, *button1, *button2;
	GSList *group;
	struct draw_info *info;
	struct crazychat *cc;

	cc = (struct crazychat*)plugin->info->extra_info;
	assert(cc);

	/* create widgets */

	/* creating the config frame */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	/* make the network configuration frame */
	frame = pidgin_make_frame(ret, _("Network Configuration"));
	gtk_widget_show(frame);

	/* add boxes for packing purposes */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(frame), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	/* add widgets to row 1 */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	label = gtk_label_new(_("TCP port"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 15);
	gtk_widget_show(label);
	adj = (GtkAdjustment*)gtk_adjustment_new(DEFAULT_CC_PORT, 1,
			G_MAXUSHORT, 1, 1000, 0);
	spinner = gtk_spin_button_new(adj, 1, 0);
	g_signal_connect(G_OBJECT(spinner), "value_changed",
			G_CALLBACK(tcp_port_cb), cc);
	gtk_box_pack_start(GTK_BOX(hbox), spinner, FALSE, FALSE, 0);
	gtk_widget_show(spinner);
	label = gtk_label_new(_("UDP port"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 15);
	gtk_widget_show(label);
	adj = (GtkAdjustment*)gtk_adjustment_new(DEFAULT_CC_PORT, 1,
			G_MAXUSHORT, 1, 1000, 0);
	spinner = gtk_spin_button_new(adj, 1, 0);
	g_signal_connect(G_OBJECT(spinner), "value_changed",
			G_CALLBACK(udp_port_cb), cc);
	gtk_box_pack_start(GTK_BOX(hbox), spinner, FALSE, FALSE, 0);
	gtk_widget_show(spinner);

	/* make the feature configuration frame */
	frame = pidgin_make_frame(ret, _("Feature Calibration"));
	gtk_widget_show(frame);

	/* add hbox for packing purposes */
	hbox = gtk_hbox_new(TRUE, 40);
	gtk_box_pack_start(GTK_BOX(frame), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	/* add feature calibration options */

	/* add vbox for packing purposes */
	vbox = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	/* add enabled / disabled */
	button1 = gtk_radio_button_new_with_label(NULL, _("Enabled"));
	gtk_box_pack_start(GTK_BOX(vbox), button1, TRUE, TRUE, 0);
	gtk_widget_show(button1);

	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button1));
	button2 = gtk_radio_button_new_with_label(group, _("Disabled"));
	gtk_box_pack_start(GTK_BOX(vbox), button2, TRUE, TRUE, 0);
	gtk_widget_show(button2);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button1),
			cc->features_state);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button2),
			!cc->features_state);
	g_signal_connect_swapped(G_OBJECT(button1), "toggled",
			G_CALLBACK(features_enable_cb), cc);

	/* add vbox for packing purposes */
	vbox = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	/* add calibrate button */
	button = gtk_button_new_with_label("Calibrate");
	gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, FALSE, 0);
	gtk_widget_show(button);

	gtk_widget_show(ret);

	return ret;
}

static void tcp_port_cb(GtkSpinButton *spin, struct crazychat *cc)
{
	assert(spin);
	assert(cc);
	cc->tcp_port = gtk_spin_button_get_value_as_int(spin);
	Debug("New tcp port: %d\n", cc->tcp_port);
}

static void udp_port_cb(GtkSpinButton *spin, struct crazychat *cc)
{
	assert(spin);
	assert(cc);
	cc->udp_port = gtk_spin_button_get_value_as_int(spin);
	Debug("New udp port: %d\n", cc->udp_port);
}

static void features_enable_cb(struct crazychat *cc)
{
	Debug("Changing features state\n");
	cc->features_state = !cc->features_state;
	if (cc->features_state) {
		cc->input_data = init_input(cc);
	} else {
		if (cc->input_data) {
			gtk_widget_destroy(cc->input_data->widget);
		}
	}
}

static gboolean cc_signed_on(PurpleConnection *gc, void *plugin)
{
	struct crazychat *extra;
	void *conv_handle;

	assert(plugin);
	extra = (struct crazychat*)((PurplePlugin*)plugin)->info->extra_info;
	purple_signal_disconnect
	    (purple_connections_get_handle(), "signed-on",
	     plugin, PURPLE_CALLBACK(cc_signed_on));
	purple_signal_connect(PIDGIN_BLIST
			    (purple_get_blist()),
			    "drawing-menu", plugin,
			    PURPLE_CALLBACK(cc_buddy_menu), NULL);
	conv_handle = purple_conversations_get_handle();
	purple_signal_connect(conv_handle, "received-im-msg", plugin,
		PURPLE_CALLBACK(receive_im_cb), extra);
	purple_signal_connect(conv_handle, "displaying-im-msg", plugin,
		PURPLE_CALLBACK(display_im_cb), extra);
	return FALSE;
}

static gboolean plugin_load(PurplePlugin *plugin)
{
	PurpleBuddyList *buddy_list;
	void *conv_handle;

	if (cc_init_gtk_gl())
		return FALSE;

	cc_init(&cc_info);
	buddy_list = purple_get_blist();
	if (buddy_list) {
		purple_signal_connect(PIDGIN_BLIST
				    (buddy_list),
				    "drawing-menu", plugin,
				    PURPLE_CALLBACK(cc_buddy_menu), NULL);
		conv_handle = purple_conversations_get_handle();
		purple_signal_connect(conv_handle, "received-im-msg", plugin,
			PURPLE_CALLBACK(receive_im_cb), &cc_info);
		purple_signal_connect(conv_handle, "displaying-im-msg", plugin,
			PURPLE_CALLBACK(display_im_cb), &cc_info);
	} else {
		purple_signal_connect
		    (purple_connections_get_handle(), "signed-on",
		     plugin, PURPLE_CALLBACK(cc_signed_on), plugin);
	}

	Debug("CrazyChat plugin loaded.\n");

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	void *conv_handle;
	struct crazychat *extra;
	assert(plugin);
	extra = (struct crazychat*) plugin->info->extra_info;
	cc_destroy(extra);
	conv_handle = purple_conversations_get_handle();
	purple_signal_disconnect(PIDGIN_BLIST
			       (purple_get_blist()),
			       "drawing-menu", plugin,
			       PURPLE_CALLBACK(cc_buddy_menu));
	purple_signal_disconnect(conv_handle, "received-im", plugin,
			       PURPLE_CALLBACK(receive_im_cb));
	purple_signal_disconnect(conv_handle, "displaying-im-msg", plugin,
			       PURPLE_CALLBACK(display_im_cb));
	Debug("CrazyChat plugin unloaded.\n");
	return TRUE;
}

static void init_plugin(PurplePlugin *plugin)
{
	gtk_gl_init(NULL, NULL);
	memset(&cc_info, 0, sizeof(cc_info));
	Debug("CrazyChat plugin initialized\n");
}

PURPLE_INIT_PLUGIN(crazychat, init_plugin, info)
