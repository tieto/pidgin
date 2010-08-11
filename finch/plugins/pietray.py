#!/usr/bin/env python

# This is a dbus script to show a docklet for Finch. This should work
# for any 'compatible' purple client.
#
# By 'compatible', I mean any client that sets and updates the
# "unseen-count" data on the conversations.
#
# It allows doing the following things:
#    - It allows changing status.
#    - It shows the current status and info about unread messages in
#      the tooltip.
#    - It can blink on unread IM/Chat messages, and it allows canging
#      the preference for that.
# 
# It requires GTK+ 2.10 or above, since it uses GtkStatusIcon.
# 
# Sadrul <sadrul@pidgin.im>

import pygtk
pygtk.require("2.0")
import gtk
import dbus, gobject, dbus.glib
import os # to get the pkg-config output

bus = dbus.SessionBus()
obj = bus.get_object(
    "im.pidgin.purple.PurpleService", "/im/pidgin/purple/PurpleObject")
purple = dbus.Interface(obj, "im.pidgin.purple.PurpleInterface")

def pack_image_label(menu, image, label):
	item = gtk.ImageMenuItem(label)
	if image:
		img = gtk.Image()
		img.set_from_stock(image, 1)
		item.set_image(img)
	menu.append(item)
	return item

def activate_primitive_status(item, status):
	saved = purple.PurpleSavedstatusFindTransientByTypeAndMessage(status, "")
	if not saved:
		saved = purple.PurpleSavedstatusNew("", status)
	purple.PurpleSavedstatusActivate(saved)

def activate_popular_status(item, time):
	saved = purple.PurpleSavedstatusFindByCreationTime(time)
	if saved:
		purple.PurpleSavedstatusActivate(saved)

def generate_status_menu(menu):
	item = gtk.MenuItem("Available")
	item.connect("activate", activate_primitive_status, 2)
	menu.append(item)

	item = gtk.MenuItem("Away")
	item.connect("activate", activate_primitive_status, 5)
	menu.append(item)

	item = gtk.MenuItem("Invisible")
	item.connect("activate", activate_primitive_status, 4)
	menu.append(item)

	item = gtk.MenuItem("Offline")
	item.connect("activate", activate_primitive_status, 1)
	menu.append(item)

	menu.append(gtk.MenuItem())

	popular = purple.PurpleSavedstatusesGetPopular(10)
	for pop in popular:
		title = purple.PurpleSavedstatusGetTitle(pop).replace('_', '__')
		item = gtk.MenuItem(title)
		item.set_data("timestamp", purple.PurpleSavedstatusGetCreationTime(pop))
		item.connect("activate", activate_popular_status, purple.PurpleSavedstatusGetCreationTime(pop))
		menu.append(item)

def toggle_pref(item, pref):
	purple.PurplePrefsSetBool(pref, item.get_active())

def quit_finch(item, null):
	# XXX: Ask first
	purple.PurpleCoreQuit()
	gtk.main_quit()

def close_docklet(item, null):
	gtk.main_quit()

def popup_menu(icon, button, tm, none):
	menu = gtk.Menu()

	#item = gtk.ImageMenuItem(gtk.STOCK_QUIT)
	#item.connect("activate", quit_finch, None)
	#menu.append(item)

	item = gtk.ImageMenuItem(gtk.STOCK_CLOSE)
	item.connect("activate", close_docklet, None)
	menu.append(item)

	menu.append(gtk.MenuItem())

	item = gtk.CheckMenuItem("Blink for unread IM")
	item.set_active(purple.PurplePrefsGetBool("/plugins/dbus/docklet/blink/im"))
	item.connect("activate", toggle_pref, "/plugins/dbus/docklet/blink/im")
	menu.append(item)

	item = gtk.CheckMenuItem("Blink for unread Chats")
	item.set_active(purple.PurplePrefsGetBool("/plugins/dbus/docklet/blink/chat"))
	item.connect("activate", toggle_pref, "/plugins/dbus/docklet/blink/chat")
	menu.append(item)

	menu.append(gtk.MenuItem())

	#item = pack_image_label(menu, None, "Change Status...")
	item = gtk.MenuItem("Change Status...")
	menu.append(item)
	submenu = gtk.Menu()
	item.set_submenu(submenu)
	generate_status_menu(submenu)

	menu.show_all()
	menu.popup(None, None, None, button, tm)

def get_status_message():
	status = purple.PurpleSavedstatusGetCurrent()
	msg = purple.PurpleSavedstatusGetMessage(status)
	if msg and len(msg) > 0:
		text = msg + " "
	else:
		text = ""
	text = text + "(" + {
		2: "Available",
		5: "Away",
		4: "Invisible",
		1: "Offline"
	}[purple.PurpleSavedstatusGetType(status)] + ")"
	return text

def detect_unread_conversations():
	im = purple.PurplePrefsGetBool("/plugins/dbus/docklet/blink/im")
	chat = purple.PurplePrefsGetBool("/plugins/dbus/docklet/blink/chat")
	tooltip = ""
	blink = False
	if im and chat:
		convs = purple.PurpleGetConversations()
	elif im:
		convs = purple.PurpleGetIms()
	elif chat:
		convs = purple.PurpleGetChats()
	else:
		convs = None
	for conv in convs:
		count = purple.PurpleConversationGetData(conv, "unseen-count")
		if count and count > 0:
			blink = True
			tooltip = tooltip + "\n" + purple.PurpleConversationGetName(conv) + " (" + str(count) + ")"
	t.set_from_file(path + "/share/pixmaps/pidgin.png")
	if blink:
		# I hate this icon
		# t.set_from_file(path + "/share/pixmaps/pidgin/tray/22/tray-message.png")
		tooltip = "\nUnread Messages:" + tooltip
	# There's going to be some way to expose the client's display name in 2.1.0.
	# Use that instead of hardcoding Finch here.
	t.set_tooltip("Finch: " + get_status_message() + tooltip)
	t.set_blinking(blink)

def conversation_updated(conv, type):
	detect_unread_conversations()

def savedstatus_changed(new, old):
	# Change the icon for status perhaps?
	detect_unread_conversations()

def init_prefs():
	if not purple.PurplePrefsExists("/plugins/dbus/docklet/blink"):
		purple.PurplePrefsAddNone("/plugins")
		purple.PurplePrefsAddNone("/plugins/dbus")
		purple.PurplePrefsAddNone("/plugins/dbus/docklet")
		purple.PurplePrefsAddNone("/plugins/dbus/docklet/blink")
		purple.PurplePrefsAddBool("/plugins/dbus/docklet/blink/im", True)
		purple.PurplePrefsAddBool("/plugins/dbus/docklet/blink/chat", True)

pkg = os.popen("pkg-config --variable=prefix pidgin")
path = pkg.readline().rstrip()

bus.add_signal_receiver(conversation_updated,
  dbus_interface="im.pidgin.purple.PurpleInterface",
  signal_name="ConversationUpdated")

bus.add_signal_receiver(savedstatus_changed,
  dbus_interface="im.pidgin.purple.PurpleInterface",
  signal_name="SavedstatusChanged")

t = gtk.StatusIcon()
t.connect("popup-menu", popup_menu, None)

try:
	init_prefs()
	detect_unread_conversations()
	gtk.main ()
except:
	dialog = gtk.Dialog("pietray: Error", None, gtk.DIALOG_NO_SEPARATOR | gtk.DIALOG_MODAL, ("Close", gtk.RESPONSE_CLOSE))
	dialog.set_resizable(False)
	dialog.vbox.pack_start(gtk.Label("There was some error. Perhaps a purple client is not running."), False, False, 0)
	dialog.show_all()
	dialog.run()

