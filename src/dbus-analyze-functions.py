# This programs takes a C header file as the input and produces:
#
# with option --mode=xml:  xml dbus specification 
# with option --mode=c:    C wrappers 
#

import re
import string
import sys


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
            "gaim_conv_placement_get_fnc_id", "gaim_conv_placement_add_fnc",
            "gaim_presence_add_list"]

stringlists = []

pointer = "#pointer#"
myexception = "My Exception"

def ctopascal(name):
    newname = ""
    for word in name.split("_"):
        newname += word.capitalize()
    return newname

class Parameter:
    def __init__(self, type, name):
        self.name = name
        self.type = type

    def fromtokens(tokens, parameternumber = -1):
        if len(tokens) == 0:
            raise myexception
        if (len(tokens) == 1) or (tokens[-1] == pointer):
            if parameternumber >= 0:
                return Parameter(tokens, "param%i" % parameternumber)
            else:
                raise myexception
        else:
            return Parameter(tokens[:-1], tokens[-1])
                    
    fromtokens = staticmethod(fromtokens)

class Binding:
    def __init__(self, functiontext, paramtexts):
        self.function = Parameter.fromtokens(functiontext.split())

        if self.function.name in excluded:
            raise myexception

        self.params = []
        for i in range(len(paramtexts)):
            self.params.append(Parameter.fromtokens(paramtexts[i].split(), i))

        self.call = "%s(%s)" % (self.function.name,
                                ", ".join([param.name for param in self.params]))
        
    
    def process(self):
        for param in self.params:
            self.processinput(param.type, param.name)

        self.processoutput(self.function.type, "RESULT")
        self.flush()
        

    def processinput(self, type, name):
        const = False
        if type[0] == "const":
            type = type[1:]
            const = True

        if len(type) == 1:
            # simple types (int, gboolean, etc.) and enums
            if (type[0] in simpletypes) or (type[0].startswith("Gaim")):
                return self.inputsimple(type, name)


            # va_list, replace by NULL
            if type[0] == "va_list":
                return self.inputvalist(type, name)

        # pointers ... 
        if (len(type) == 2) and (type[1] == pointer):
            # strings
            if type[0] == "char":
                if const:
                    return self.inputstring(type, name)
                else:
                    raise myexception

            elif type[0] == "GHashTable":
                return self.inputhash(type, name)
                
            # known object types are transformed to integer handles
            elif type[0].startswith("Gaim"):
                return self.inputgaimstructure(type, name)

            # unknown pointers are always replaced with NULL
            else:
                return self.inputpointer(type, name)
                return

        raise myexception

   
    def processoutput(self, type, name):
        # the "void" type is simple ...
        if type == ["void"]:
            return self.outputvoid(type, name)

        const = False
        if type[0] == "const":
            type = type[1:]
            const = True

        # a string
        if type == ["char", pointer]:
            return self.outputstring(type, name, const)

        # simple types (ints, booleans, enums, ...)
        if (len(type) == 1) and \
               ((type[0] in simpletypes) or (type[0].startswith("Gaim"))):
            return self.outputsimple(type, name)

        # pointers ...
        if (len(type) == 2) and (type[1] == pointer):

            # handles
            if type[0].startswith("Gaim"):
                return self.outputgaimstructure(type, name)

            if type[0] in ["GList", "GSList"]:
                return self.outputlist(type, name)

        raise myexception
    

class ClientBinding (Binding):
    def __init__(self, functiontext, paramtexts, knowntypes, headersonly):
        Binding.__init__(self, functiontext, paramtexts)
        self.knowntypes = knowntypes
        self.headersonly = headersonly
        self.paramshdr = []
        self.decls = []
        self.inputparams = []
        self.outputparams = []
        self.returncode = []

    def flush(self):
        print "%s %s(%s)" % (self.functiontype, self.function.name,
                             ", ".join(self.paramshdr)),

        if self.headersonly:
            print ";"
            return

        print "{"

        for decl in self.decls:
            print decl

        print 'dbus_g_proxy_call(gaim_proxy, "%s", NULL,' % ctopascal(self.function.name)
        
        for type_name in self.inputparams:
            print "%s, %s, " % type_name,
        print "G_TYPE_INVALID,"

        for type_name in self.outputparams:
            print "%s, &%s, " % type_name,
        print "G_TYPE_INVALID);"
        
        for code in self.returncode:
            print code

        print "}\n"
        

    def definegaimstructure(self, type):
        if (self.headersonly) and (type[0] not in self.knowntypes):
            print "struct _%s;" % type[0]
            print "typedef struct _%s %s;" % (type[0], type[0])
            self.knowntypes.append(type[0])

    # fixme: import the definitions of the enumerations from gaim
    # header files
    def definegaimenum(self, type):
        if (self.headersonly) and (type[0] not in self.knowntypes) \
               and (type[0] not in simpletypes):
            print "typedef int %s;" % type[0]
            self.knowntypes.append(type[0])
       
    def inputsimple(self, type, name):
        self.paramshdr.append("%s %s" % (type[0], name))
        self.inputparams.append(("G_TYPE_INT", name))
        self.definegaimenum(type)

    def inputvalist(self, type, name):
        self.paramshdr.append("va_list %s_NULL" % name)

    def inputstring(self, type, name):
        self.paramshdr.append("const char *%s" % name)
        self.inputparams.append(("G_TYPE_STRING", name))
        
    def inputgaimstructure(self, type, name):
        self.paramshdr.append("const %s *%s" % (type[0], name))
        self.inputparams.append(("G_TYPE_INT", "GPOINTER_TO_INT(%s)" % name))
        self.definegaimstructure(type)

    def inputpointer(self, type, name):
        name += "_NULL"
        self.paramshdr.append("const %s *%s" % (type[0], name))
        self.inputparams.append(("G_TYPE_INT", "0"))
        
    def inputhash(self, type, name):
        self.paramshdr.append("const GHashTable *%s" % name)
        self.inputparams.append(('dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_STRING)', name))

    def outputvoid(self, type, name):
        self.functiontype = "void"

    def outputstring(self, type, name, const):
        self.functiontype = "char*"
        self.decls.append("char *%s = NULL;" % name)
        self.outputparams.append(("G_TYPE_STRING", name))
#        self.returncode.append("NULLIFY(%s);" % name)
        self.returncode.append("return %s;" % name);

    def outputsimple(self, type, name):
        self.functiontype = type[0]
        self.decls.append("%s %s = 0;" % (type[0], name))
        self.outputparams.append(("G_TYPE_INT", name))
        self.returncode.append("return %s;" % name);
        self.definegaimenum(type)

    # we could add "const" to the return type but this would probably
    # be a nuisance
    def outputgaimstructure(self, type, name):
        name = name + "_ID"
        self.functiontype = "%s*" % type[0]
        self.decls.append("int %s = 0;" % name)
        self.outputparams.append(("G_TYPE_INT", "%s" % name))
        self.returncode.append("return (%s*) GINT_TO_POINTER(%s);" % (type[0], name));
        self.definegaimstructure(type)

    def outputlist(self, type, name):
        self.functiontype = "%s*" % type[0]
        self.decls.append("GArray *%s;" % name)
        self.outputparams.append(('dbus_g_type_get_collection("GArray", G_TYPE_INT)', name))
        self.returncode.append("return garray_int_to_%s(%s);" %
                               (type[0].lower(), name));

 
class ServerBinding (Binding):
    def __init__(self, functiontext, paramtexts):
        Binding.__init__(self, functiontext, paramtexts)
        self.dparams = ""
        self.cparams = []
        self.cdecls  = []
        self.ccode  = []
        self.cparamsout = []
        self.ccodeout = []
        self.argfunc = "dbus_message_get_args"

    def flush(self):
        print "static DBusMessage*"
        print "%s_DBUS(DBusMessage *message_DBUS, DBusError *error_DBUS) {" % \
              self.function.name
        
        print "DBusMessage *reply_DBUS;"

        for decl in self.cdecls:
            print decl

        print "%s(message_DBUS, error_DBUS, " % self.argfunc,
        for param in self.cparams:
            print "DBUS_TYPE_%s, &%s," % param,
        print "DBUS_TYPE_INVALID);"

        print "CHECK_ERROR(error_DBUS);"

        for code in self.ccode:
            print code

        print "reply_DBUS =  dbus_message_new_method_return (message_DBUS);"

        print "dbus_message_append_args(reply_DBUS, ",
        for param in self.cparamsout:
            if type(param) is str:
                print "%s, " % param
            else:
                print "DBUS_TYPE_%s, &%s, " % param,
        print "DBUS_TYPE_INVALID);"

        for code in self.ccodeout:
            print code

        print "return reply_DBUS;\n}\n"


    def addstring(self, *items):
        for item in items:
            self.dparams += item + r"\0"

    def addintype(self, type, name):
        self.addstring("in", type, name)

    def addouttype(self, type, name):
        self.addstring("out", type, name)


    # input parameters

    def inputsimple(self, type, name):
        self.cdecls.append("dbus_int32_t %s;" % name)
        self.cparams.append(("INT32", name))
        self.addintype("i", name)

    def inputvalist(self, type, name):
        self.cdecls.append("va_list %s;" % name);
        self.ccode.append("%s = NULL;" % name);

    def inputstring(self, type, name):
        self.cdecls.append("const char *%s;" % name)
        self.cparams.append(("STRING", name))
        self.ccode  .append("NULLIFY(%s);" % name)
        self.addintype("s", name)

    def inputhash(self, type, name):
        self.argfunc = "gaim_dbus_message_get_args"
        self.cdecls.append("DBusMessageIter %s_ITER;" % name)
        self.cdecls.append("GHashTable *%s;" % name)
        self.cparams.append(("ARRAY", "%s_ITER" % name))
        self.ccode.append("%s = gaim_dbus_iter_hash_table(&%s_ITER, error_DBUS);" \
                     % (name, name))
        self.ccode.append("CHECK_ERROR(error_DBUS);")
        self.ccodeout.append("g_hash_table_destroy(%s);" % name)
        self.addintype("a{ss}", name)

    def inputgaimstructure(self, type, name):
        self.cdecls.append("dbus_int32_t %s_ID;" %  name)
        self.cdecls.append("%s *%s;" % (type[0], name))
        self.cparams.append(("INT32", name + "_ID"))
        self.ccode.append("GAIM_DBUS_ID_TO_POINTER(%s, %s_ID, %s, error_DBUS);"  % \
                       (name, name, type[0]))
        self.addintype("i", name)

    def inputpointer(self, type, name):
        self.cdecls.append("dbus_int32_t %s_NULL;" %  name)
        self.cdecls .append("%s *%s;" % (type[0], name))
        self.cparams.append(("INT32", name + "_NULL"))
        self.ccode  .append("%s = NULL;" % name)
        self.addintype("i", name)

    # output parameters

    def outputvoid(self, type, name):
        self.ccode.append("%s;" % self.call) # just call the function

    def outputstring(self, type, name, const):
        self.cdecls.append("const char *%s;" % name)
        self.ccode.append("%s = null_to_empty(%s);" % (name, self.call))
        self.cparamsout.append(("STRING", name))
        self.addouttype("s", name)
        if not const:
            self.ccodeout.append("g_free(%s);" % name)

    def outputsimple(self, type, name):
        self.cdecls.append("dbus_int32_t %s;" % name)
        self.ccode.append("%s = %s;" % (name, self.call))
        self.cparamsout.append(("INT32", name))
        self.addouttype("i", name)

    def outputgaimstructure(self, type, name):
        self.cdecls.append("dbus_int32_t %s;" % name)
        self.ccode .append("GAIM_DBUS_POINTER_TO_ID(%s, %s, error_DBUS);" % (name, self.call))
        self.cparamsout.append(("INT32", name))
        self.addouttype("i", name)

    # GList*, GSList*, assume that list is a list of objects

    # fixme: at the moment, we do NOT free the memory occupied by
    # the list, we should free it if the list has NOT been declared const

    # fixme: we assume that this is a list of objects, not a list
    # of strings

    def outputlist(self, type, name):
        self.cdecls.append("dbus_int32_t %s_LEN;" % name)
        self.ccodeout.append("g_free(%s);" % name)

        if self.function.name in stringlists:
            self.cdecls.append("char **%s;" % name)
            self.ccode.append("%s = gaim_%s_to_array(%s, FALSE, &%s_LEN);" % \
                         (name, type[0], self.call, name))
            self.cparamsout.append("DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &%s, %s_LEN" \
                          % (name, name))
            self.addouttype("as", name)
        else:
            self.cdecls.append("dbus_int32_t *%s;" % name)
            self.ccode.append("%s = gaim_dbusify_%s(%s, FALSE, &%s_LEN);" % \
                         (name, type[0], self.call, name))
            self.cparamsout.append("DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &%s, %s_LEN" \
                              % (name, name))
            self.addouttype("ai", name)


class BindingSet:
    regexp = r"^(\w[^()]*)\(([^()]*)\)\s*;\s*$";

    def __init__(self, inputfile, fprefix):
        self.inputiter = iter(inputfile)
        self.functionregexp = \
             re.compile("^%s(\w[^()]*)\(([^()]*)\)\s*;\s*$" % fprefix)    


                
    def process(self):
        print "/* Generated by %s.  Do not edit! */" % sys.argv[0]

        for line in self.inputiter:
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
                newline = self.inputiter.next().strip()
                if len(newline) == 0:
                    break
                myline += " " + newline

            # is this a function declaration?
            thematch = self.functionregexp.match(
                myline.replace("*", " " + pointer + " "))

            if thematch is None:
                continue

            functiontext = thematch.group(1)
            paramstext = thematch.group(2).strip()

            if (paramstext == "void") or (paramstext == ""):
                paramtexts = []
            else:
                paramtexts = paramstext.split(",")

            try:
                self.processfunction(functiontext, paramtexts)
            except myexception:
                sys.stderr.write(myline + "\n")
            except:
                sys.stderr.write(myline + "\n")
                raise

        self.flush()

class ServerBindingSet (BindingSet):
    def __init__(self, inputfile, fprefix):
        BindingSet.__init__(self, inputfile, fprefix)
        self.functions = []


    def processfunction(self, functiontext, paramtexts):
        binding = ServerBinding(functiontext, paramtexts)
        binding.process()
        self.functions.append((binding.function.name, binding.dparams))
        
    def flush(self):
        print "static GaimDBusBinding bindings_DBUS[] = { "
        for function, params in self.functions:
            print '{"%s", "%s", %s_DBUS},' % \
                  (ctopascal(function), params, function)

        print "{NULL, NULL}"
        print "};"

        print "#define GAIM_DBUS_REGISTER_BINDINGS(handle) gaim_dbus_register_bindings(handle, bindings_DBUS)"
        
class ClientBindingSet (BindingSet):
    def __init__(self, inputfile, fprefix, headersonly):
        BindingSet.__init__(self, inputfile, fprefix)
        self.functions = []
        self.knowntypes = []
        self.headersonly = headersonly

    def processfunction(self, functiontext, paramtexts):
        binding = ClientBinding(functiontext, paramtexts, self.knowntypes, self.headersonly)
        binding.process()

    def flush(self):
        pass

# Main program

options = {}

for arg in sys.argv[1:]:
    if arg[0:2] == "--":
        mylist = arg[2:].split("=",1)
        command = mylist[0]
        if len(mylist) > 1:
            options[command] = mylist[1]
        else:
            options[command] = None

if "export-only" in options:
    fprefix = "DBUS_EXPORT\s+"
else:
    fprefix = ""

sys.stderr.write("%s: Functions not exported:\n" % sys.argv[0])

if "client" in options:
    bindings = ClientBindingSet(sys.stdin, fprefix,
                                options.has_key("headers"))
else:
    bindings = ServerBindingSet(sys.stdin, fprefix)
bindings.process()




