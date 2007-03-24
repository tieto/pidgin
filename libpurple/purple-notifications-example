#!/usr/bin/env python

# This is a simple gaim notification server.
# It shows notifications when your buddy signs on or you get an IM message.
#
# This script requires Python 2.4 and PyGTK bindings
#
# Note that all function names are resolved dynamically, no
# gaim-specific library is needed.

import dbus
import dbus.glib
import dbus.decorators
import gobject
import os

def ensureimconversation(conversation, account, name):
    if conversation != 0:
        return conversation
    else:
        # 1 = GAIM_CONV_IM 
        return gaim.GaimConversationNew(1, account, name)

def receivedimmsg(account, name, message, conversation, flags):
    buddy = gaim.GaimFindBuddy(account, name)
    if buddy != 0:
        alias = gaim.GaimBuddyGetAlias(buddy)
    else:
        alias = name

    text = "%s says %s" % (alias, message)
    code = os.spawnlp(os.P_WAIT, "xmessage", "xmessage", "-buttons",
                      "'So what?','Show me',Close,Abuse", text)

    if code == 101:                     # so what?
        pass
    else:
        conversation = ensureimconversation(conversation, account, name)

    if code == 102:                     # show me
        window = gaim.GaimConversationGetWindow(conversation)
        gaim.GaimConvWindowRaise(window)

    if code == 103:                     # close 
        gaim.GaimConversationDestroy(conversation)

    if code == 104:                     # abuse
        im = gaim.GaimConversationGetImData(conversation)
        gaim.GaimConvImSend(im, "Go away you f...")
                                 
        
def buddysignedon(buddyid):
    alias = gaim.GaimBuddyGetAlias(buddyid)
    text = "%s is online" % alias

    code = os.spawnlp(os.P_WAIT, "xmessage", "xmessage", "-buttons",
                      "'So what?','Let's talk'", text)

    if code == 101:                     # so what?
        pass

    if code == 102:                     # talk
        name = gaim.GaimBuddyGetName(buddyid)
        account = gaim.GaimBuddyGetAccount(buddyid)
        gaim.GaimConversationNew(1, account, name)
    

bus = dbus.SessionBus()
obj = bus.get_object("net.sf.gaim.GaimService", "/net/sf/gaim/GaimObject")
gaim = dbus.Interface(obj, "net.sf.gaim.GaimInterface")

bus.add_signal_receiver(receivedimmsg,
                        dbus_interface = "net.sf.gaim.GaimInterface",
                        signal_name = "ReceivedImMsg")

bus.add_signal_receiver(buddysignedon,
                        dbus_interface = "net.sf.gaim.GaimInterface",
                        signal_name = "BuddySignedOn")

print "This is a simple gaim notification server."
print "It shows notifications when your buddy signs on or you get an IM message."

loop = gobject.MainLoop()
loop.run()


