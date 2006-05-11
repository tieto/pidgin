#!/usr/bin/python

import dbus
import re
import urllib
import sys

import xml.dom.minidom 

xml.dom.minidom.Element.all   = xml.dom.minidom.Element.getElementsByTagName

obj = dbus.SessionBus().get_object("net.sf.gaim.GaimService", "/net/sf/gaim/GaimObject")
gaim = dbus.Interface(obj, "net.sf.gaim.GaimInterface")

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
    if protocol == "aim" or protocol == "icq":
        protocol = "oscar"
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
        current = gaim.GaimSavedstatusGetCurrent()

        if "status" in params:
            status_id = params["status"]
            status_type = gaim.GaimPrimitiveGetTypeFromId(status_id)
        else:
            status_type = gaim.GaimSavedStatusGetType(current)
            status_id = gaim.GaimPrimitiveGetIdFromType(status_type)

        if "message" in params:
            message = params["message"];
        else:
            message = gaim.GaimSavedstatusGetMessage(current)

        if "account" in params:
            accounts = [cgaim.GaimAccountsFindAny(accountname, protocol)]

            for account in accounts:
                status = gaim.GaimAccountGetStatus(account, status_id)
                type = gaim.GaimStatusGetType(status)
                gaim.GaimSavedstatusSetSubstatus(current, account, type, message)
                gaim.GaimSavedstatusActivateForAccount(current, account)
        else:
            accounts = gaim.GaimAccountsGetAllActive()
            saved = gaim.GaimSavedstatusNew("", status_type)
            gaim.GaimSavedstatusSetMessage(saved, message)
            gaim.GaimSavedstatusActivate(saved)

        return None

    elif command == "getinfo":
        account = findaccount(accountname, protocol)
        connection = cgaim.GaimAccountGetConnection(account)
        return gaim.ServGetInfo(connection, params["screenname"])

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


if len(sys.argv) == 1:
    print """This program uses DBus to communicate with gaim.

Usage:

    %s "command1" "command2" ...

Each command is of one of the three types:

    [protocol:]commandname?param1=value1&param2=value2&...
    FunctionName?param1=value1&param2=value2&...
    FunctionName(value1,value2,...)

The second and third form are provided for completeness but their use
is not recommended; use gaim-send or gaim-send-async instead.  The
second form uses introspection to find out the parameter names and
their types, therefore it is rather slow.

Examples of commands:

    jabber:goim?screenname=testone@localhost&message=hi
    jabber:gochat?room=TestRoom&server=conference.localhost
    jabber:getinfo?screenname=testone@localhost
    jabber:addbuddy?screenname=my friend

    setstatus?status=away&message=don't disturb
    quit

    GaimAccountsFindConnected?name=&protocol=prpl-jabber
    GaimAccountFindConnected(,prpl-jabber)
""" % sys.argv[0]

for arg in sys.argv[1:]:
    print execute(arg)
    
    
