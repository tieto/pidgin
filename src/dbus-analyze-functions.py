# This programs takes a C header file as the input and produces:
#
# with option --mode=xml:  xml dbus specification 
# with option --mode=c:    C wrappers 
#



import re
import string
import sys

options = {}

for arg in sys.argv[1:]:
    if arg[0:2] == "--":
        mylist = arg[2:].split("=",1)
        command = mylist[0]
        if len(mylist) > 1:
            options[command] = mylist[1]
        else:
            options[command] = None

# list of object types

# objecttypes = []

# for objecttype in file("dbus-auto-structs.txt"):
#     objecttypes.append(objecttype.strip())

# a dictionary of simple types
# each TYPE maps into a pair (dbus-type-name, compatible-c-type-name)
# if compatible-c-type-name is None then it is the same as TYPE

# simpletypes = {
#     "int" : ("i", None),
#     "gint" : ("i", None),
#     "guint" : ("u", None),
#     "gboolean" : ("i", "int")
#     }

simpletypes = ["int", "gint", "guint", "gboolean"]

# for enum in file("dbus-auto-enums.txt"):
#     simpletypes[enum.strip()] = ("i", "int")

# functions that shouldn't be exported 

excluded = ["gaim_accounts_load", "gaim_account_set_presence",
            "gaim_conv_placement_get_fnc_id", "gaim_conv_placement_add_fnc"]

pointer = "#pointer#"

functions = []

dparams = ""
cparams = []
cparamsout = []
cdecls  = []
ccode  = []
ccodeout  = []

myexception = "My Exception"

def ctopascal(name):
    newname = ""
    for word in name.split("_"):
        newname += word.capitalize()
    return newname

def c_print(function):
    print "static DBusMessage *%s_DBUS(DBusMessage *message_DBUS, DBusError *error_DBUS) {" \
          % function

    print "DBusMessage *reply_DBUS;"

    for decl in cdecls:
        print decl

    print "dbus_message_get_args(message_DBUS, error_DBUS, ",
    for param in cparams:
        print "DBUS_TYPE_%s, &%s," % param,
    print "DBUS_TYPE_INVALID);"

    print "CHECK_ERROR(error_DBUS);"

    for code in ccode:
        print code

    print "reply_DBUS =  dbus_message_new_method_return (message_DBUS);"

    print "dbus_message_append_args(reply_DBUS, ",
    for param in cparamsout:
        if type(param) is str:
            print "%s, " % param
        else:
            print "DBUS_TYPE_%s, &%s, " % param,
    print "DBUS_TYPE_INVALID);"

    for code in ccodeout:
        print code

    print "return reply_DBUS;\n}\n"

    functions.append((function, dparams))

def c_clear():
    global cparams, cdecls, ccode, cparamsout, ccodeout, dparams
    dparams = ""
    cparams = []
    cdecls  = []
    ccode  = []
    cparamsout = []
    ccodeout = []


def addstring(*items):
    global dparams
    for item in items:
        dparams += item + r"\0"

def addintype(type, name):
    addstring("in", type, name)

def addouttype(type, name):
    addstring("out", type, name)

def printdispatchtable():
    print "static GaimDBusBinding bindings_DBUS[] = { "
    for function, params in functions:
        print '{"%s", "%s", %s_DBUS},' % (ctopascal(function), params, function)
    print "{NULL, NULL}"
    print "};"
    
    print "#define GAIM_DBUS_REGISTER_BINDINGS(handle) gaim_dbus_register_bindings(handle, bindings_DBUS)"

# processing an input parameter

def inputvar(mytype, name):
    global ccode, cparams, cdecls
    const = False
    if mytype[0] == "const":
        mytype = mytype[1:]
        const = True

    # simple types (int, gboolean, etc.) and enums
    if (len(mytype) == 1) and \
           ((mytype[0] in simpletypes) or (mytype[0].startswith("Gaim"))):
        cdecls.append("dbus_int32_t %s;" % name)
        cparams.append(("INT32", name))
        addintype("i", name)
        return

    # pointers ...

    if (len(mytype) == 2) and (mytype[1] == pointer):
        # strings
        if mytype[0] == "char":
            if const:
                cdecls.append("const char *%s;" % name)
                cparams.append(("STRING", name))
                ccode  .append("NULLIFY(%s);" % name)
                addintype("s", name)
                return
            else:
                raise myexception

        # known object types are transformed to integer handles
        elif mytype[0].startswith("Gaim"):
            cdecls.append("dbus_int32_t %s_ID;" %  name)
            cdecls.append("%s *%s;" % (mytype[0], name))
            cparams.append(("INT32", name + "_ID"))
            ccode.append("GAIM_DBUS_ID_TO_POINTER(%s, %s_ID, %s, error_DBUS);"  % \
                           (name, name, mytype[0]))
            addintype("i", name)
            return

        # unknown pointers are always replaced with NULL
        else:
            cdecls.append("dbus_int32_t %s_NULL;" %  name)
            cdecls .append("%s *%s;" % (mytype[0], name))
            cparams.append(("INT32", name + "_NULL"))
            ccode  .append("%s = NULL;" % name)
            addintype("i", name)
            return

    raise myexception

            

# processing an output parameter

def outputvar(mytype, name, call):
    # the "void" type is simple ...
    if mytype == ["void"]:
        ccode.append("%s;" % call) # just call the function
        return

    # a constant string
    if mytype == ["const", "char", pointer]:
        cdecls.append("const char *%s;" % name)
        ccode.append("%s = null_to_empty(%s);" % (name, call))
        cparamsout.append(("STRING", name))
        addouttype("s", name)
        return

    # simple types (ints, booleans, enums, ...)
    if (len(mytype) == 1) and \
           ((mytype[0] in simpletypes) or (mytype[0].startswith("Gaim"))):
        cdecls.append("dbus_int32_t %s;" % name)
        ccode.append("%s = %s;" % (name, call))
        cparamsout.append(("INT32", name))
        addouttype("i", name)
        return
            
    # pointers ...
    if (len(mytype) == 2) and (mytype[1] == pointer):

        # handles
        if mytype[0].startswith("Gaim"):
            cdecls.append("dbus_int32_t %s;" % name)
            ccode .append("GAIM_DBUS_POINTER_TO_ID(%s, %s, error_DBUS);" % (name, call))
            cparamsout.append(("INT32", name))
            addouttype("i", name)
            return

        # GList*, GSList*, assume that list is a list of objects
        # not a list of strings!!!
        # this does NOT release memory occupied by the list
        if mytype[0] in ["GList", "GSList"]:
            cdecls.append("dbus_int32_t %s_LEN;" % name)
            cdecls.append("dbus_int32_t *%s;" % name)
            ccode.append("%s = gaim_dbusify_%s(%s, FALSE, &%s_LEN);" % \
                         (name, mytype[0], call, name))
            cparamsout.append("DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &%s, %s_LEN" \
                              % (name, name))
            ccodeout.append("g_free(%s);" % name)
            addouttype("ai", name)
            return

    raise myexception



def processfunction(functionparam, paramlist):
    c_clear()

    ftokens = functionparam.split()
    functiontype, function = ftokens[:-1], ftokens[-1]

    if function in excluded:
        return

    origfunction = function
    function = function.lower()

    names = []
    for param in paramlist:
        tokens = param.split()
        if len(tokens) < 2:
            raise myexception
        type, name = tokens[:-1], tokens[-1]
        inputvar(type, name)
        names.append(name)

    outputvar(functiontype, "RESULT",
              "%s(%s)" % (origfunction, ", ".join(names)))

    c_print(function)



    
print "/* Generated by %s.  Do not edit! */" % sys.argv[0]



regexp = r"^(\w[^()]*)\(([^()]*)\)\s*;\s*$";


if "export-only" in options:
    fprefix = "DBUS_EXPORT\s+"
else:
    fprefix = ""
    
functionregexp = re.compile("^%s(\w[^()]*)\(([^()]*)\)\s*;\s*$" % fprefix)

inputiter = iter(sys.stdin)

for line in inputiter:
        words = line.split()
        if len(words) == 0:             # empty line
            continue
        if line[0] == "#":              # preprocessor directive
            continue
        if words[0] in ["typedef", "struct", "enum", "static"]:
            continue

        # accumulate lines until the parentheses are balance or an
        # empty line has been encountered
        myline = line.strip()
        while myline.count("(") > myline.count(")"):
            newline = inputiter.next().strip()
            if len(newline) == 0:
                break
            myline += " " + newline

        # is this a function declaration?
        thematch = functionregexp.match(
            myline.replace("*", " " + pointer + " "))

        if thematch is None:
            continue

        function = thematch.group(1)
        parameters = thematch.group(2).strip()

        if (parameters == "void") or (parameters == ""):
            paramlist = []
        else:
            paramlist = parameters.split(",")

        try:
            processfunction(function, paramlist)
        except myexception:
            sys.stderr.write(myline + "\n")
        except:
            sys.stderr.write(myline + "\n")
            raise

printdispatchtable()

