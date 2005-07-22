# This program takes a C header/source as the input and produces
#
# with --keyword=enum: the list of all enums
# with --keyword=struct: the list of all structs
#
# the output styles:
#
# --enum    DBUS_POINTER_NAME1,
#           DBUS_POINTER_NAME2,
#           DBUS_POINTER_NAME3, 
# 
# --list    NAME1
#           NAME2
#           NAME3
# 


import re
import sys

myinput = iter(sys.stdin)

def outputenum(name):
    print "DBUS_POINTER_%s," % name

def outputdeclare(name):
    print "DECLARE_TYPE(%s, NONE);" % name

def outputtext(name):
    print name

myoutput = outputtext
keyword = "struct"

for arg in sys.argv[1:]:
    if arg[0:2] == "--":
        mylist = arg[2:].split("=")
        command = mylist[0]
        if len(mylist) > 1:
            value = mylist[1]
        else:
            value = None
            
    if command == "enum":
        myoutput = outputenum
    if command == "declare":
        myoutput = outputdeclare
    if command == "list":
        myoutput = outputtext
    if command == "keyword":
        keyword = value
        

structregexp1 = re.compile(r"^(typedef\s+)?%s\s+\w+\s+(\w+)\s*;" % keyword)
structregexp2 = re.compile(r"^(typedef\s+)?%s" % keyword)
structregexp3 = re.compile(r"^}\s+(\w+)\s*;")

for line in myinput:
    match = structregexp1.match(line)
    if match is not None:
        myoutput(match.group(2))        
        continue

    match = structregexp2.match(line)
    if match is not None:
        while True:
            line = myinput.next()
            match = structregexp3.match(line)
            if match is not None:
                myoutput(match.group(1))
                break
            if line[0] not in [" ", "\t", "{", "\n"]:
                break
        



    
    
