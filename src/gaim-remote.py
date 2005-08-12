#!/usr/bin/python

import dbus
import re
import urllib
import sys

import xml.dom.minidom 

xml.dom.minidom.Element.all   = xml.dom.minidom.Element.getElementsByTagName

obj = dbus.SessionBus().get_object("org.gaim.GaimService", "/org/gaim/GaimObject")
gaim = dbus.Interface(obj, "org.gaim.GaimInterface")

class CheckedObject:
    def __init__(self, obj):
        self.obj = obj

    def __getattr__(self, attr):
        return CheckedAttribute(self, attr)

class CheckedAttribute:
    def __init__(self, cobj, attr):
        self.cobj = cobj
        self.attr = attr
        
    def __call__(self, *args):
        result = self.cobj.obj.__getattr__(self.attr)(*args)
        if result == 0:
            raise "Error: " + self.attr + " " + str(args) + " returned " + str(result)
        return result
            
cgaim = CheckedObject(gaim)

urlregexp = r"^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?"

def extendlist(list, length, fill):
    if len(list) < length:
        return list + [fill] * (length - len(list))
    else:
        return list

def convert(value):
    try:
        return int(value)
    except:
        return value

def findaccount(accountname, protocolname):
    try:
        # prefer connected accounts
        account = cgaim.GaimAccountsFindConnected(accountname, protocolname)
        return account
    except:
        # try to get any account and connect it
        account = cgaim.GaimAccountsFindAny(accountname, protocolname)
        print gaim.GaimAccountGetUsername(account)
        gaim.GaimAccountSetStatusVargs(account, "online", 1)
        gaim.GaimAccountConnect(account)
        return account
    

def execute(uri):
    match = re.match(urlregexp, uri)
    protocol = match.group(2)
    if protocol is not None:
        protocol = "prpl-" + protocol
    command = match.group(5)
    paramstring = match.group(7) 
    params = {}
    if paramstring is not None:
        for param in paramstring.split("&"):
            key, value = extendlist(param.split("=",1), 2, "")
            params[key] = urllib.unquote(value)

    accountname = params.get("account", "")

    if command == "goim":
        print params
        account = findaccount(accountname, protocol)
        conversation = cgaim.GaimConversationNew(1, account, params["screenname"])
        if "message" in params:
            im = cgaim.GaimConversationGetImData(conversation)
            gaim.GaimConvImSend(im, params["message"])
        return None

    elif command == "gochat":
        account = findaccount(accountname, protocol)
        connection = cgaim.GaimAccountGetConnection(account)
        return gaim.ServJoinChat(connection, params)

    elif command == "addbuddy":
        account = findaccount(accountname, protocol)
        return cgaim.GaimBlistRequestAddBuddy(account, params["screenname"],
                                              params.get("group", ""), "")

    elif command == "setstatus":
        if "account" in params:
            accounts = [cgaim.GaimAccountsFindAny(accountname, protocol)]
        else:
            accounts = gaim.GaimAccountsGetAllActive()

        for account in accounts:
            status = cgaim.GaimAccountGetStatus(account, params["status"])
            for key, value in params.items():
                if key not in ["status", "account"]:
                    gaim.GaimStatusSetAttrString(status, key, value)
            gaim.GaimAccountSetStatusVargs(account, params["status"], 1)
        return None

    elif command == "getinfo":
        account = findaccount(accountname, protocol)
        connection = cgaim.GaimAccountGetConnection(account)
        gaim.ServGetInfo(connection, params["screenname"])

    elif command == "quit":
        return gaim.GaimCoreQuit()

    elif command == "uri":
        return None

    else:
        match = re.match(r"(\w+)\s*\(([^)]*)\)", command)
        if match is not None:
            name = match.group(1)
            argstr = match.group(2)
            if argstr == "":
                args = []
            else:
                args = argstr.split(",")
            fargs = []
            for arg in args:
                fargs.append(convert(arg.strip()))
            return gaim.__getattr__(name)(*fargs)
        else:
            # introspect the object to get parameter names and types
            # this is slow because the entire introspection info must be downloaded
            data = dbus.Interface(obj, "org.freedesktop.DBus.Introspectable").\
                   Introspect()
            introspect = xml.dom.minidom.parseString(data).documentElement
            for method in introspect.all("method"):
                if command == method.getAttribute("name"):
                    methodparams = []
                    for arg in method.all("arg"):
                        if arg.getAttribute("direction") == "in":
                            value = params[arg.getAttribute("name")]
                            type = arg.getAttribute("type")
                            if type == "s":
                                methodparams.append(value)
                            elif type == "i":
                                methodparams.append(int(value))
                            else:
                                raise "Don't know how to handle type \"%s\"" % type
                            return gaim.__getattr__(command)(*methodparams)
            raise "Unknown command: %s" % command

def example_code_do_not_call():
    execute("jabber:addbuddy?screenname=friend")
    execute("setstatus?status=away&message=don't disturb")

    account = execute("GaimAccountsFindConnected?name=&protocol=")
    execute("GaimConversationNew?type=1&account=%i&name=testone@localhost" % account)

    execute("jabber:addbuddy?screenname=friend")
    execute("jabber:goim?screenname=testone@localhost&message=hi")

    execute("jabber:gochat?room=TestRoom&server=conference.localhost")
    execute("jabber:goim?screenname=testone@localhost&message=hi")





for arg in sys.argv[1:]:
    print execute(arg)
    
    
