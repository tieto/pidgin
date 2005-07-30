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

keyword = "struct"
pattern = "%s"

for arg in sys.argv[1:]:
    if arg[0:2] == "--":
        mylist = arg[2:].split("=",1)
        command = mylist[0]
        if len(mylist) > 1:
            value = mylist[1]
        else:
            value = None
            
    if command == "pattern":
        pattern = value
    if command == "keyword":
        keyword = value
        

structregexp1 = re.compile(r"^(typedef\s+)?%s\s+\w+\s+(\w+)\s*;" % keyword)
structregexp2 = re.compile(r"^(typedef\s+)?%s" % keyword)
structregexp3 = re.compile(r"^}\s+(\w+)\s*;")

myinput = iter(sys.stdin)

for line in myinput:
    match = structregexp1.match(line)
    if match is not None:
        print pattern % match.group(2)
        continue

    match = structregexp2.match(line)
    if match is not None:
        while True:
            line = myinput.next()
            match = structregexp3.match(line)
            if match is not None:
                print pattern % match.group(1)
                break
            if line[0] not in [" ", "\t", "{", "\n"]:
                break
        



    
    
