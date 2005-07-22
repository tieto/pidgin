#!/usr/bin/env python

# this is an example of a client that communicates with gaim using DBUS
#
# requires Python 2.4 and PyGTK bindings
#
# note that all function names are resolved dynamically, no
# gaim-specific library is needed

import dbus
import dbus.glib
import dbus.decorators
import gobject
import os

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
    if code == 102:                     # let's talk
        name = gaim.GaimBuddyGetName(buddyid)
        account = gaim.GaimBuddyGetAccount(buddyid)
        gaim.GaimConversationNew(1, account, name)
    

def talkto(buddyname, accountname, protocolname):
    account = gaim.GaimAccountsFindConnected(accountname, protocolname)
    if account != 0:                    
        gaim.GaimConversationNew(1, account, buddyname)
    

bus = dbus.SessionBus()
obj = bus.get_object("org.gaim.GaimService", "/org/gaim/GaimObject")
gaim = dbus.Interface(obj, "org.gaim.GaimInterface")

bus.add_signal_receiver(receivedimmsg,
                        dbus_interface = "org.gaim.GaimInterface",
                        signal_name = "ReceivedImMsg")
bus.add_signal_receiver(buddysignedon,
                        dbus_interface = "org.gaim.GaimInterface",
                        signal_name = "BuddySignedOn")


# Tell the remote object to emit the signal

talkto("testone@localhost", "", "prpl-jabber")

loop = gobject.MainLoop()
loop.run()


