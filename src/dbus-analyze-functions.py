# This programs takes a C header file as the input and produces:
#
# with option --mode=xml:  xml dbus specification 
# with option --mode=c:    C wrappers 
#



import re
import string
import sys

# mode: "c" or "xml"

mode = None

for arg in sys.argv[1:]:
    if arg.startswith("--mode="):
        mode = arg[len("--mode="):]

# list of object types

objecttypes = []

for objecttype in file("dbus-auto-structs.txt"):
    objecttypes.append(objecttype.strip())

# a dictionary of simple types
# each TYPE maps into a pair (dbus-type-name, compatible-c-type-name)
# if compatible-c-type-name is None then it is the same as TYPE

simpletypes = {
    "int" : ("i", None),
    "gint" : ("i", None),
    "guint" : ("u", None),
    "gboolean" : ("i", "int")
    }

for enum in file("dbus-auto-enums.txt"):
    simpletypes[enum.strip()] = ("i", "int")

# functions that shouldn't be exported 

excluded = ["gaim_accounts_load", "gaim_account_set_presence"]

pointer = "#pointer#"
prefix = "gaim_object_"

cparams = []
cdecls  = []
ccode  = []
dargs  = []

myexception = "My Exception"


def ctopascal(name):
    newname = ""
    for word in name.split("_"):
        newname += word.capitalize()
    return newname

def dbus_print(function):
    print '<method name="%s">' % ctopascal(function)

    for name, type, direction in dargs:
        print '<arg type="%s" name="%s" direction="%s" />' % \
              (type, name, direction)

    print '</method>'
    print 

def dbus_clear():
    global dargs
    dargs = []

def c_print(function):
    print "static gboolean %s%s(GaimObject *gaim_object," % (prefix, function),

    for param in cparams:
        print "%s," % param,

    print "GError **error) {"

    for decl in cdecls:
        print decl

    for code in ccode:
        print code

    print "return TRUE;\n}\n"
   
def c_clear():
    global cparams, cdecls, ccode
    cparams = []
    cdecls  = []
    ccode  = []


# processing an input parameter

def inputvar(mytype, name):
    global dargs, ccode, cparams, cdecls
    const = False
    if mytype[0] == "const":
        mytype = mytype[1:]
        const = True

    # simple types (int, gboolean, etc.) and enums
    if (len(mytype) == 1) and (mytype[0] in simpletypes):
        dbustype, ctype = simpletypes[mytype[0]]
        dargs.append((name, dbustype, "in"))
        if ctype is None:
            cparams.append(mytype[0] + " " + name)
        else:
            cparams.append(ctype + " " + name + "_ORIG")
            cdecls .append("%s %s;\n" % (mytype[0], name))
            ccode  .append("%s = (%s) %s_ORIG;\n" % \
                           (name, mytype[0], name))
        return

    # pointers ...

    if (len(mytype) == 2) and (mytype[1] == pointer):
        # strings
        if mytype[0] == "char":
            if const:
                dargs  .append((name, "s", "in"))
                cparams.append("const char *" + name)
                ccode  .append("NULLIFY(%s);" % name)
                return
            else:
                raise myexception

        # known object types are transformed to integer handles
        elif mytype[0] in objecttypes:
            dargs  .append((name, "i", "in"))
            cparams.append("int " + name + "_ID")
            cdecls .append("%s *%s;" % (mytype[0], name))
            ccode  .append("GAIM_DBUS_ID_TO_POINTER(%s, %s_ID, %s);"  % \
                           (name, name, mytype[0]))
            return

        # unknown pointers are always replaced with NULL
        else:
            dargs  .append((name, "i", "in"))
            cparams.append("int " + name + "_NULL")
            cdecls .append("%s *%s;" % (mytype[0], name))
            ccode  .append("%s = NULL;" % name)
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
        dargs  .append((name, "s", "out"))
        cparams.append("char **%s" % name)
        ccode  .append("*%s = g_strdup(null_to_empty(%s));" % (name, call))
        return

    # simple types (ints, booleans, enums, ...)
    if (len(mytype) == 1) and (mytype[0] in simpletypes): 
        dbustype, ctype = simpletypes[mytype[0]]

        if ctype is None:
            ctype = mytype[0]
            
        dargs  .append((name, dbustype, "out"))
        ccode  .append("*%s = %s;" % (name, call))
        cparams.append("%s *%s" % (ctype, name))
        return
            
    # pointers ...
    if (len(mytype) == 2) and (mytype[1] == pointer):

        # handles
        if mytype[0] in objecttypes:
            dargs  .append((name, "i", "out"))
            cparams.append("int *%s" % name)
            ccode  .append("GAIM_DBUS_POINTER_TO_ID(*%s, %s);" % (name, call))
            return

        # GList*, GSList*, assume that list is a list of objects
        # not a list of strings!!!
        if mytype[0] in ["GList", "GSList"]:
            dargs  .append((name, "ai", "out"))
            cparams.append("GArray **%s" % name)
            ccode  .append("*%s = gaim_dbusify_%s(%s, TRUE);" % \
                           (name, mytype[0],call))
            return

    raise myexception



def processfunction(functionparam, paramlist):
    dbus_clear()
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

    if mode == "c":
        c_print(function)

    if mode == "xml":
        dbus_print(function)


if mode == "c":
    print "/* Generated by %s.  Do not edit! */" % sys.argv[0]

if mode == "xml":
    print "<!-- Generated by %s.  Do not edit! -->" % sys.argv[0]

functionregexp = re.compile(r"^(\w[^()]*)\(([^()]*)\)\s*;\s*$")

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





