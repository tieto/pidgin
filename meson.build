project('pidgin', 'c')

# UPDATING VERSION NUMBERS FOR RELEASES
#
# purple_micro_version += 1
#
# If any functions have been added to libpurple, Pidgin, or Finch:
#   purple_micro_version = 0
#   purple_minor_version += 1
#   purple_lt_current += 1
#
# If backwards compatibility has been broken in libpurple, Pidgin, or Finch:
#   purple_micro_version = 0
#   purple_minor_version = 0
#   purple_major_version += 1;
#   purple_lt_current += 1
#
# purple_version_suffix should be similar to one of the following:
#   For beta releases:          'beta2'
#   For code under development: 'devel'
#   For production releases:    ''
#
#
# If any code has changed in libgnt:
#   gnt_micro_version += 1
#
# If any functions have been added to libgnt:
#   gnt_micro_version = 0
#   gnt_minor_version += 1
#   gnt_lt_current += 1
#
# If backwards compatibility has been broken in libgnt:
#   gnt_micro_version = 0
#   gnt_minor_version = 0
#   gnt_major_version += 1;
#   gnt_lt_current += 1
#
# gnt_version_suffix should be similar to one of the following:
#   For beta releases:          'beta2'
#   For code under development: 'devel'
#   For production releases:    ''
#
# Make sure to update finch/libgnt/configure.ac with libgnt version changes.
#
purple_lt_current = 20
purple_major_version = 3
purple_minor_version = 0
purple_micro_version = 0
purple_version_suffix = 'devel'
purple_version = '@0@.@1@.@2@'.format(purple_major_version,
                                      purple_minor_version,
                                      purple_micro_version)
purple_display_version = '@0@@1@'.format(purple_version,
                                         purple_version_suffix)

# the last version for Finch 2 was 2.8.10,
# the first version for Finch 3 was 2.9.0
gnt_lt_current = 9
gnt_major_version = 2
gnt_minor_version = 9
gnt_micro_version = 0
gnt_version_suffix = 'devel'
gnt_version = '@0@.@1@.@2@'.format(gnt_major_version,
                                   gnt_minor_version,
                                   gnt_micro_version)
gnt_display_version = '@0@@1@'.format(gnt_version,
                                      gnt_version_suffix)


add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')
conf = configuration_data()
version_conf = configuration_data()

conf.set('VERSION', '"@0@"'.format(purple_display_version))

version_conf.set('PURPLE_MAJOR_VERSION', purple_major_version)
version_conf.set('PURPLE_MINOR_VERSION', purple_minor_version)
version_conf.set('PURPLE_MICRO_VERSION', purple_micro_version)
version_conf.set('PURPLE_VERSION', purple_display_version)
version_conf.set('PURPLE_API_VERSION',
                 purple_lt_current - purple_minor_version)

PURPLE_LT_VERSION_INFO = '@0@.@1@.@2@'.format(purple_lt_current,
                                              purple_micro_version,
                                              purple_minor_version)
version_conf.set('PURPLE_LT_VERSION_INFO', PURPLE_LT_VERSION_INFO)

version_conf.set('GNT_MAJOR_VERSION', gnt_major_version)
version_conf.set('GNT_MINOR_VERSION', gnt_minor_version)
version_conf.set('GNT_MICRO_VERSION', gnt_micro_version)
version_conf.set('GNT_VERSION', gnt_display_version)
version_conf.set('GNT_API_VERSION', gnt_lt_current - gnt_minor_version)

GNT_LT_VERSION_INFO = '@0@.@1@.@2@'.format(gnt_lt_current,
                                           gnt_micro_version,
                                           gnt_minor_version)
version_conf.set('GNT_LT_VERSION_INFO', GNT_LT_VERSION_INFO)

sedpath = find_program('sed')
xxdpath = find_program('xxd')

# Storing configure arguments
# conf.set('CONFIG_ARGS', ac_configure_args)

# Checks for programs.
compiler = meson.get_compiler('c')
#cxx_compiler = meson.get_compiler('cpp')

if compiler.has_function('alloca')  # FIXME: Probably not enough.
	conf.set('HAVE_ALLOCA_H', true)
else
	error('alloca could not be found')
endif

# Check for Sun compiler
SUNCC = compiler.compiles('void main() {__SUNPRO_C;};')

# Check for Win32
if host_machine.system() == 'windows'
	is_win32 = true
	is_not_win32 = false
	LIBS += [declare_dependency(link_with : 'ws2_32')]
	dnsapi = declare_dependency(link_with : 'dnsapi')
	PLUGIN_LDFLAGS = '-avoid-version -no-undefined'
	WINDRES = generator(find_program('windres'),
	                    output : '@BASENAME@.o',
	                    arguments : ['-i', '@INPUT@', '-o', '@OUTPUT@'])
	conf.set('IS_WIN32_CROSS_COMPILED', true)
	conf.set('WIN32_LEAN_AND_MEAN', true)
else
	is_win32 = false
	is_not_win32 = true
	dnsapi = declare_dependency()
	PLUGIN_LDFLAGS = '-avoid-version'
	if not compiler.has_header('signal.h')
		error('signal.h is required.')
	endif
endif
IS_WIN32 = is_win32
#AC_SUBST([PLUGIN_LDFLAGS])

# Define *_LIBS
PURPLE_LIBS='\$(top_builddir)/libpurple/libpurple.la \$(GLIB_LIBS)'
PIDGIN_LIBS='\$(top_builddir)/pidgin/libpidgin.la \$(GTK_LIBS)'
FINCH_LIBS='\$(top_builddir)/finch/libfinch.la \$(top_builddir)/finch/libgnt/libgnt.la \$(GLIB_LIBS)'

# Checks for header files.
# AC_HEADER_STDC:
stdc = true
foreach h : ['stdlib.h', 'stdarg.h', 'string.h', 'float.h']
	if not compiler.has_header(h)
		stdc = false
	endif
endforeach
if stdc
	code = '''#include <string.h>
void func() { memchr("Compile me.\n", 'C', 2); }
'''
	stdc = stdc and compiler.compiles(code, name : 'string.h declares mem* functions')
endif
if stdc
	code = '''#include <stdlib.h>
void func() { free(NULL); }
'''
	stdc = stdc and compiler.compiles(code, name : 'stdlib.h declares free and malloc family')
endif
if stdc
	# 'ctype.h' macros work on characters with the high bit set
#	code = '''#include <ctype.h>
#
#'''
#	stdc = stdc and compiler.compiles(code, name : 'string.h declares memory functions')
endif
conf.set('STDC_HEADERS', stdc)
# AC_HEADER_SYS_WAIT:
conf.set('HAVE_SYS_WAIT_H', compiler.has_header('sys/wait.h'))

foreach h : ['fcntl.h', 'unistd.h', 'locale.h', 'stdint.h']
	if compiler.has_header(h)
		conf.set('HAVE_' + h.to_upper().underscorify(), true)
	else
		error(h + ' is required.')
	endif
endforeach

# Checks for typedefs, structures, and compiler characteristics.
time_t_size = compiler.sizeof('time_t',
                              prefix : '''
#include <stdio.h>
#include <time.h>
''')
conf.set('SIZEOF_TIME_T', time_t_size)

#AC_C_BIGENDIAN

conf.set('USE_WIN32_FHS',
         is_win32 and get_option('win32-dirs') == 'fhs')

# Check for directories
if is_win32
	if get_option('win32-dirs') == 'fhs'
		conf.set('WIN32_FHS_BINDIR', get_option('bindir'))
		conf.set('WIN32_FHS_LIBDIR', get_option('libdir'))
		conf.set('WIN32_FHS_DATADIR', get_option('datadir'))
		conf.set('WIN32_FHS_SYSCONFDIR', get_option('sysconfdir'))
		conf.set('WIN32_FHS_LOCALEDIR', get_option('localedir'))

		purple_libdir = 'wpurple_lib_dir("purple-@0@")'.format(purple_major_version)
		pidgin_libdir = 'wpurple_lib_dir("pidgin-@0@")'.format(purple_major_version)
		finch_libdir = 'wpurple_lib_dir("finch-@0@")'.format(purple_major_version)
	else
		purple_libdir = 'wpurple_lib_dir(NULL)'
		pidgin_libdir = 'wpurple_lib_dir(NULL)'
		finch_libdir = 'wpurple_lib_dir(NULL)'
	endif

	purple_datadir = 'wpurple_data_dir()'
	purple_sysconfdir = 'wpurple_sysconf_dir()'
	purple_localedir = 'wpurple_locale_dir()'
else
	purple_datadir = '"@0@"'.format(get_option('datadir'))
	purple_sysconfdir = '"@0@"'.format(get_option('sysconfdir'))
	purple_localedir = '"@0@"'.format(get_option('localedir'))

	common_libdir = get_option('libdir')
	purple_libdir = '"@0@/purple-@1@"'.format(common_libdir, purple_major_version)
	pidgin_libdir = '"@0@/pidgin-@1@"'.format(common_libdir, purple_major_version)
	finch_libdir = '"@0@/finch-@1@"'.format(common_libdir, purple_major_version)
endif
conf.set('PURPLE_DATADIR', purple_datadir)
conf.set('PURPLE_LIBDIR', purple_libdir)
conf.set('PIDGIN_LIBDIR', pidgin_libdir)
conf.set('FINCH_LIBDIR', finch_libdir)
conf.set('PURPLE_SYSCONFDIR', purple_sysconfdir)
conf.set('PURPLE_LOCALEDIR', purple_localedir)

# FIXME:
PURPLE_PLUGINDIR = '@0@/purple-@1@'.format(get_option('libdir'), purple_major_version)
conf.set('PURPLE_PLUGINDIR', '"@0@"'.format(PURPLE_PLUGINDIR))
PIDGIN_PLUGINDIR = '@0@/pidgin-@1@'.format(get_option('libdir'), purple_major_version)
conf.set('PIDGIN_PLUGINDIR', '"@0@"'.format(PIDGIN_PLUGINDIR))
FINCH_PLUGINDIR = '@0@/finch-@1@'.format(get_option('libdir'), purple_major_version)
conf.set('FINCH_PLUGINDIR', '"@0@"'.format(FINCH_PLUGINDIR))

# Checks for library functions.
foreach func : ['strdup', 'strstr', 'atexit', 'setlocale']
	conf.set('HAVE_' + func.to_upper(),
	         compiler.has_function(func))
endforeach
# Checks for getopt in standard library
if compiler.has_function('getopt_long')
	getopt_files = []
else
	getopt_files = ['getopt.c', 'getopt1.c']
endif

# Check for inet_aton
if is_win32
	if not compiler.has_function('inet_aton')
		if not compiler.has_function('inet_aton', args : '-lresolv')
			error('inet_aton not found')
		endif
	endif
endif
if compiler.has_function('__res_query', args : '-lresolv')
	conf.set('HAVE_LIBRESOLV', true)
endif
if compiler.has_function('gethostent', args : '-lnsl')
	conf.set('HAVE_LIBNSL', true)
endif
if is_win32
	conf.set('HAVE_GETADDRINFO', true)
	conf.set('HAVE_INET_NTOP', true)
else
	if not compiler.has_function('socket')
		if not compiler.has_function('socket', args : '-lsocket')
			error('socket not found')
		endif
	endif
	# If all goes well, by this point the previous two checks will have
	# pulled in -lsocket and -lnsl if we need them.
	if compiler.has_function('getaddrinfo')
		conf.set('HAVE_GETADDRINFO', true)
	else
		if compiler.has_function('getaddrinfo', args : '-lsocket -lnsl')
			conf.set('HAVE_GETADDRINFO', true)
			# FIXME: LIBS += declare_dependency(link_with : ['socket', 'nsl'])
		endif
	endif
	if compiler.has_function('inet_ntop')
		conf.set('HAVE_INET_NTOP', true)
	endif
endif
if compiler.has_function('getifaddrs')
	conf.set('HAVE_GETIFADDRS', true)
endif
# Check for socklen_t (in Unix98)
code = '''
#include <sys/types.h>
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#endif
socklen_t x;

int main() {}
'''
if not compiler.compiles(code, name : 'socklen_t')
	code = '''
#include <sys/types.h>
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#endif
int accept(int, struct sockaddr *, size_t *);
int main() {}
'''
	if compiler.compiles(code, name : 'socklen_t is size_t')
        conf.set('socklen_t', 'size_t')
	else
        conf.set('socklen_t', 'int')
	endif
endif

# Some systems do not have sa_len field for struct sockaddr.
if compiler.has_member('struct sockaddr', 'sa_len',
                       prefix : '''
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#endif
''')
	conf.set('HAVE_STRUCT_SOCKADDR_SA_LEN', true)
endif

# Check for v6-only sockets
code = '''
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
#  include <netinet/in.h>
#endif
void main() {IPV6_V6ONLY;};
'''
if compiler.compiles(code)
	conf.set('HAVE_IPV6_V6ONLY', true)
endif

# to prevent the g_stat()/g_unlink() crash,
# (09:50:07) Robot101: LSchiere2: it's easy. +LC_SYS_LARGEFILE somewhere in configure.ac
#AC_SYS_LARGEFILE

# FreeBSD doesn't have libdl, dlopen is provided by libc
if compiler.has_function('dlopen')
	LIBDL = ''
else
	if compiler.has_function('dlopen', args : '-ldl')
		LIBDL = '-ldl'
	endif
endif

# Windows and Haiku do not use libm for the math functions, they are part
# of the C library
math = compiler.find_library('m')

code = '''
#include <stdio.h>

int main(int argc, char *argv[])
{
	int fd;

	fd = fileno(stdout);

	return !(fd > 0);
}
'''
result = compiler.run(code, name : 'fileno()')
conf.set('HAVE_FILENO', result.returncode() == 0)

code = '''
#include <time.h>
#include <stdio.h>

int main()
{
	char buf[64];
	time_t t = time(NULL);

	if (strftime(buf, sizeof(buf), "%z", localtime(&t)) != 5)
		return 1;

	fprintf(stderr, "strftime(\"%%z\") yields: \"%s\"\n", buf);

	return !((buf[0] == '-' || buf[0] == '+') &&
	         (buf[1] >= '0' && buf[1] <= '9') &&
	         (buf[2] >= '0' && buf[2] <= '9') &&
	         (buf[3] >= '0' && buf[3] <= '9') &&
	         (buf[4] >= '0' && buf[4] <= '9')
	        );
}
'''
result = compiler.run(code, name : 'the %z format string in strftime()')
conf.set('HAVE_STRFTIME_Z_FORMAT', result.returncode() == 0)

# before gettexting, in case iconv matters
if host_machine.system() == 'darwin'
	if compiler.has_function('res_query', args : '-lresolv')
		conf.set('HAVE_LIBRESOLV', true)
	endif

	if compiler.has_header('CoreFoundation/CoreFoundation.h')
		if compiler.has_header('IOKit/IOKitLib.h')
			conf.set('HAVE_IOKIT', true)
			# FIXME: LIBS="$LIBS -framework IOKit -framework CoreFoundation"
		endif
	endif

	if run_command('test', '-d', '/sw').returncode() == 0
		# FIXME: Not done...
		#CPPFLAGS="$CPPFLAGS -I/sw/include"
		#LDFLAGS="$LDFLAGS -L/sw/lib"
	endif
endif

# #######################################################################
# # Disable creation and installation of translation files
# #######################################################################

INSTALL_I18N = get_option('nls')

if INSTALL_I18N
	subdir('po')
endif

# #######################################################################
# # Check for GLib 2.34 (required)
# #######################################################################
glib = dependency('glib-2.0', version : '>= 2.23.0')
gio = dependency('gio-2.0')
gobject = dependency('gobject-2.0')
gthread = dependency('gthread-2.0')
# glib = all of the above

GLIB_GENMARSHAL = find_program('glib-genmarshal')
GLIB_MKENUMS = find_program('glib-mkenums')

GTESTER = find_program('gtester')
#GLIB_TESTS

if get_option('extraversion') != ''
	DISPLAY_VERSION = '@0@-@1@'.format(meson.project_version(),
	                                   get_option('extraversion'))
else
	DISPLAY_VERSION = meson.project_version()
endif
conf.set('DISPLAY_VERSION', '"@0@"'.format(DISPLAY_VERSION))

force_deps = not get_option('missing-dependencies')

with_x = get_option('x') and is_not_win32

# #######################################################################
# # Check for GTK+ 2.18 and other things used by the GTK UI
# #######################################################################
enable_screensaver = get_option('screensaver')
enable_sm = get_option('sm')
enable_startup_notification = get_option('startup-notification')
enable_enchant = get_option('enchant')
enable_gevolution = get_option('gevolution')
enable_cap = get_option('cap')
enable_gestures = get_option('gestures')
enable_gcr = get_option('gcr')

#AC_PATH_XTRA
## We can't assume that $x_libraries will be set, because autoconf does not
## set it in the case when the X libraries are in a standard place.
## Ditto for $x_includes
#if test X"$x_libraries" = X"" || test X"$x_libraries" = XNONE; then
#    x_libpath_add=
#else
#    x_libpath_add="-L$x_libraries"
#fi
#if test X"$x_includes" = X"" || test X"$x_includes" = XNONE; then
#    x_incpath_add=
#else
#    x_incpath_add="-I$x_includes"
#fi

# #######################################################################
# Check Pidgin dependencies
# #######################################################################
if get_option('gtkui')
	gtk = dependency('gtk+-3.0', version : '>= 3.0.0')
	# We only really need Pango >= 1.4 for decent RTL support
	pango = dependency('pango', version : '>= 1.4.0')
	if pango.found()
		conf.set('HAVE_PANGO14', true)
	endif

	webkit = dependency('webkitgtk-3.0', version : '>= 1.3.7')

	#######################################################################
	# Check if we should compile with enchant support
	#######################################################################
	# We need enchant for spell checking dictionary enumeration,
	# because webkit1 doesn't have this.
	use_enchant = false
	if enable_enchant
		use_enchant = true
		enchant = dependency('enchant', required : force_deps)
		use_enchant = enchant.found()
		if use_enchant
			conf.set('USE_ENCHANT', true)
#			AC_SUBST(ENCHANT_CFLAGS)
#			AC_SUBST(ENCHANT_LIBS)
		endif
	endif

	#######################################################################
	# Check if we should compile with X support
	#######################################################################
	if with_x
		x11 = dependency('x11')
		if x11.found()
			conf.set('HAVE_X11', true)
		else
			if x_incpath_add != '' and x_libpath_add != ''
#				X11_LIBS='$x_libpath_add'
#				X11_CFLAGS='$x_incpath_add'
			else
				with_x = false
				if force_deps
					error('''
X11 development headers not found.
Use -Dx=false if you do not need X11 support.
''')
				endif
			endif
		endif
#		AC_SUBST(X11_LIBS)
#		AC_SUBST(X11_CFLAGS)
	else
		enable_screensaver = false
		enable_sm = false
		enable_gestures = false
	endif

	#######################################################################
	# Check for XScreenSaver
	#######################################################################
	if enable_screensaver
		if with_x
#			old_LIBS='$LIBS'
#			LIBS='$LIBS $GTK_LIBS $x_libpath_add'
#			XSS_LIBS=''
#			XSS_HEADERS=''
#			AC_CHECK_LIB(Xext, XScreenSaverRegister,[XSS_LIBS='$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_EXTRA_LIBS'],[],[-lX11 -lXext -lm])
#			AC_CHECK_LIB(Xss, XScreenSaverRegister,[XSS_LIBS='$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_LIBS $X_EXTRA_LIBS -lXss'],[],[-lX11 -lXext -lm])
#			if test 'x$XSS_LIBS' != 'x'; then
#				oldCPPFLAGS='$CPPFLAGS'
#				CPPFLAGS='$CPPFLAGS $x_incpath_add'
#				AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#					#include <X11/Xlib.h>
#					#include <X11/extensions/scrnsaver.h>
#					]], [[]])], [], [enable_screensaver=no])
#				CPPFLAGS='$oldCPPFLAGS'
#			else
#				enable_screensaver=no
#			endif
#			LIBS='$old_LIBS'

			if enable_screensaver
				conf.set('USE_SCREENSAVER', true)
#				AC_SUBST(XSS_LIBS)
			else
				if force_deps
					error('''
XScreenSaver extension development headers (libXScrnSaver-devel or libxss-dev) not found.
Use -Dscreensaver=false if you do not need XScreenSaver extension support,
this is required for detecting idle time by mouse and keyboard usage.
''')
				endif
			endif
		else
			error('X support is required to build with XScreenSaver extensions')
		endif
	endif

	#######################################################################
	# Check for X session management libs
	#######################################################################
	if enable_sm
		if with_x
			enable_sm = false
			found_sm_lib = compiler.has_function('SmcSaveYourselfDone', args : '-lSM')
			if found_sm_lib
#				oldCPPFLAGS='$CPPFLAGS'
#				CPPFLAGS='$CPPFLAGS $x_incpath_add'
				if compiler.has_header('X11/SM/SMlib.h')
#					SM_LIBS='$x_libpath_add -lSM -lICE'
					enable_sm = true
				endif
#				CPPFLAGS='$oldCPPFLAGS'
			endif

			if enable_sm
				conf.set('USE_SM', true)
#				AC_SUBST(SM_LIBS)
			else
				if force_deps
					error('''
X session management development headers not found.
Use -Dsm=false if you do not need session management support.
''')
				endif
			endif
		else
			error('X support is required to build with X session management support')
		endif
	endif

	#######################################################################
	# Check for X11 to allow the gestures plugin
	#######################################################################
	if enable_gestures
		if not with_x
			enable_gestures = false
		endif
	endif

	#######################################################################
	# Check for stuff needed by the Evolution integration plugin.
	#######################################################################
	if enable_gevolution
		evo_deps = ['libebook-1.2', 'libedata-book-1.2', 'evolution-data-server-1.2 >= 3.6']
		EVOLUTION_ADDRESSBOOK = []
		foreach dep : evo_deps
			EVOLUTION_ADDRESSBOOK.append(
				dependency(dep, required : force_deps)
			)
		endforeach
		if enable_gevolution
			conf.set('HAVE_EVOLUTION_ADDRESSBOOK', true)
#			AC_SUBST(EVOLUTION_ADDRESSBOOK_CFLAGS)
#			AC_SUBST(EVOLUTION_ADDRESSBOOK_LIBS)
		endif
	endif

	#######################################################################
	# Check for libsqlite3 (for the Contact Availability Prediction plugin)
	#######################################################################
	if enable_cap
		SQLITE3 = dependency('sqlite3', version : '>= 3.3', required : force_deps)
		enable_cap = SQLITE3.found()
	endif

	#######################################################################
	# Check for GCR for its certificate widgets
	#######################################################################
	if enable_gcr
		GCR = dependency('gcr-3', required : force_deps)
		if GCR.found()
			conf.set('ENABLE_GCR', true)
		else
			enable_gcr = false
		endif
	endif


else	# GTK
	enable_gcr = false
	enable_cap = false
	enable_gevolution = false
	enable_screensaver = false
	enable_sm = false
	enable_startup_notification = false
endif	# GTK

ENABLE_GTK = get_option('gtkui')
BUILD_GEVOLUTION = enable_gevolution
ENABLE_CAP = enable_cap
ENABLE_GESTURES = enable_gestures
ENABLE_GCR = enable_gcr


#######################################################################
# Check for ncurses and other things used by the console UI
#######################################################################
GNT_LIBS = ''
GNT_CFLAGS = ''
enable_consoleui = get_option('consoleui')
force_finch = enable_consoleui
if enable_consoleui
	if compiler.has_function('initscr', args : '-lncursesw')
		GNT_LIBS = '-lncursesw'
	else
		enable_consoleui = false
	endif
	if compiler.has_function('update_panels', args : '-lpanelw')
		GNT_LIBS += ' -lpanelw'
	else
		enable_consoleui = false
	endif

	if is_win32
		# FIXME: $host ?
		ncurses_sys_prefix = '/usr/$host/sys-root/mingw'
	else
		ncurses_sys_prefix = '/usr'
	endif

	ncurses_sys_dirs = [ncurses_sys_prefix + '/include/ncursesw',
	                    ncurses_sys_prefix + '/include']

	if enable_consoleui
		# Some distros put the headers in ncursesw/, some don't
		found_ncurses_h = false
		# FIXME: Define NCURSES_HEADERS option somewhere.
#		foreach location : NCURSES_HEADERS + ncurses_sys_dirs
#			f = location + '/ncurses.h'
#			orig_CFLAGS='$CFLAGS'
#			orig_CPPFLAGS='$CPPFLAGS'
#			CFLAGS='$CFLAGS -I$location'
#			CPPFLAGS='$CPPFLAGS -I$location'
#			AC_CHECK_HEADER($f,[
#				AC_MSG_CHECKING([if $f supports wide characters])
#				AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#					#define _XOPEN_SOURCE_EXTENDED
#					#include <$f>
#				]], [[
#					#ifndef get_wch
#					# error get_wch not found!
#					#endif
#				]])], [
#					dir=$location
#					if test x'$dir' != x'.' ; then
#						GNT_CFLAGS='-I$dir/'
#					else
#						GNT_CFLAGS=''
#					fi
#
#					found_ncurses_h=yes
#					CFLAGS='$orig_CFLAGS'
#					CPPFLAGS='$orig_CPPFLAGS'
#					AC_MSG_RESULT([yes])
#					break
#				], [
#					CFLAGS='$orig_CFLAGS'
#					CPPFLAGS='$orig_CPPFLAGS'
#					AC_MSG_RESULT([no])
#				])
#			])
#		endforeach

		if not found_ncurses_h
			GNT_LIBS = ''
			GNT_CFLAGS = ''
			enable_consoleui = false
		endif
	else
		# ncursesw was not found. Look for plain old ncurses
		enable_consoleui = true
		if compiler.has_function('initscr', args : '-lncurses')
			GNT_LIBS = '-lncurses'
		else
			enable_consoleui = false
		endif
		if compiler.has_function('update_panels', args : '-lpanel')
			GNT_LIBS += ' -lpanel'
		else
			enable_consoleui = false
		endif
		conf.set('NO_WIDECHAR', true)
		if NCURSES_HEADERS != ''
			GNT_CFLAGS = '-I' + NCURSES_HEADERS
		endif
	endif
endif

if force_finch and not enable_consoleui
	error('''

Finch will not be built. You need to install ncursesw (or ncurses) and its development headers.

''')
endif

#AC_SUBST(GNT_LIBS)
#AC_SUBST(GNT_CFLAGS)
ENABLE_GNT = enable_consoleui

#conf.set('HAVE_WCWIDTH', compiler.has_function('wcwidth'))

#######################################################################
# Check for LibXML2 (required)
#######################################################################
libxml = dependency('libxml-2.0', version : '>= 2.6.0')
libxml_okay = dependency('libxml-2.0', version : '>= 2.6.18', required : false)
if not libxml_okay.found()
message('Versions of libxml2 < 2.6.18 may contain bugs that could cause XMPP messages to be discarded.')
endif

#######################################################################
# Check for JSON-GLib (required)
#######################################################################

json = dependency('json-glib-1.0', version : '>= 0.14.0')

#######################################################################
# Check for zlib (required)
#######################################################################

zlib = dependency('zlib', version : '>= 1.2.0')

#######################################################################
# GConf schemas
#######################################################################
GCONFTOOL = find_program('gconftool-2', required : false)
USE_GCONFTOOL = GCONFTOOL.found()
#AM_GCONF_SOURCE_2

#######################################################################
# Check for GStreamer
#######################################################################

enable_gst = get_option('gstreamer')
if enable_gst
	gstreamer = dependency('gstreamer-1.0', required : force_deps)
	if gstreamer.found()
		conf.set('USE_GSTREAMER', true)
	else
		enable_gst = false
	endif
endif

#######################################################################
# Check for GStreamer Video
#######################################################################
enable_gstvideo = enable_gst and get_option('gstreamer-video')
if enable_gstvideo
	gstreamer_video = dependency('gstreamer-video-1.0',
	                             required : false)
	if gstreamer_video.found()
		conf.set('USE_GSTVIDEO', true)
	else
		enable_gstvideo = false
	endif
endif

#######################################################################
# Check for Farstream
#######################################################################
if get_option('farstream')
	farstream = dependency('farstream-0.2', version : '>= 0.2.7',
	                       required : false)
	enable_farstream = farstream.found()
else
	enable_farstream = false
endif

#######################################################################
# Check for Voice and Video support
#######################################################################
if get_option('vv')
	if enable_gst and enable_gstvideo and enable_farstream
		conf.set('USE_VV', true)
		enable_vv = true
	else
		if force_deps
			error('''
Dependencies for voice/video were not met.
Install the necessary gstreamer and farstream packages first.
Or use -Dvv=false if you do not need voice/video support.
			''')
		endif
		enable_vv = false
	endif
endif
USE_VV = enable_vv

#######################################################################
# Check for Raw data streams support in Farstream
#######################################################################
if enable_vv
	gstreamer_app = dependency('gstreamer-app-1.0',
	                           required : false)
	if gstreamer_app.found()
		conf.set('USE_GSTAPP', true)
		conf.set('HAVE_MEDIA_APPLICATION', true)
	endif
endif

#######################################################################
# Check for Internationalized Domain Name support
#######################################################################

if get_option('idn')
	idn = dependency('libidn', version : '>= 0.0.0', required : force_deps)
	conf.set('USE_IDN', idn.found())
endif

#######################################################################
# Check for Meanwhile headers (for Sametime)
#######################################################################
if get_option('meanwhile')
	MEANWHILE = dependency('meanwhile', version : '>= 1.0.0 < 2.0.0', required : force_deps)
endif
#AC_SUBST(MEANWHILE_CFLAGS)
#AC_SUBST(MEANWHILE_LIBS)

#######################################################################
# Check for Native Avahi headers (for Bonjour)
#######################################################################

enable_avahi = get_option('avahi')
if enable_avahi and is_win32
	avahiincludes = true
	avahilibs = true
elif enable_avahi
	AVAHI_CFLAGS = ''
	AVAHI_LIBS = ''

	# Attempt to autodetect Avahi
	avahi = dependency('avahi-client avahi-glib', required : false)
	if avahi.found()
		avahiincludes = true
		avahilibs = true
	else
		avahiincludes = false
		avahilibs = false
	endif

	# Override AVAHI_CFLAGS if the user specified an include dir
	if get_option('avahi-client-includes') != ''
		AVAHI_CFLAGS = '-I' + get_option('avahi-client-includes')
	endif
#	CPPFLAGS_save='$CPPFLAGS'
#	CPPFLAGS='$CPPFLAGS $AVAHI_CFLAGS'
#	AC_CHECK_HEADER(avahi-client/client.h, [avahiincludes=yes], [avahiincludes=no])
#	CPPFLAGS='$CPPFLAGS $AVAHI_CFLAGS $GLIB_CFLAGS'
#	AC_CHECK_HEADER(avahi-glib/glib-malloc.h, [avahiincludes=yes], [avahiincludes=no])
#	CPPFLAGS='$CPPFLAGS_save'
#
#	# Override AVAHI_LIBS if the user specified a libs dir
#	if test '$ac_avahi_client_libs' != 'no'; then
#		AVAHI_LIBS='-L$ac_avahi_client_libs -lavahi-common -lavahi-client -lavahi-glib '
#	fi
#	AC_CHECK_LIB(avahi-client, avahi_client_new, [avahilibs=yes], [avahilibs=no], $AVAHI_LIBS)
endif

if enable_avahi and (not avahiincludes or not avahilibs)
	enable_avahi = false
	if force_deps
		error('''
avahi development headers not found.
Use -Davahi=false if you do not need avahi (Bonjour) support.
''')
	endif
endif
#AC_SUBST(AVAHI_CFLAGS)
#AC_SUBST(AVAHI_LIBS)


#######################################################################
# Check for SILC client includes and libraries
#######################################################################
SILC_CFLAGS = ''
SILC_LIBS = ''
have_silc = false
if get_option('silc-includes') != '' or get_option('silc-libs') != ''
	silc_manual_check = true
else
	silc_manual_check = false
endif
if not silc_manual_check
	silc = dependency('silcclient', version : '>= 1.1', required : false)
	if silc.found()
		have_silc = true
		silcincludes = true
		silcclient = true
	else
		have_silc = false
	endif
else
#	if test '$ac_silc_includes' != 'no'; then
#		SILC_CFLAGS='-I$ac_silc_includes'
#	fi
#	CPPFLAGS_save='$CPPFLAGS'
#	CPPFLAGS='$CPPFLAGS $SILC_CFLAGS'
#	AC_CHECK_HEADER(silc.h, [silcincludes=yes])
#	CPPFLAGS='$CPPFLAGS_save'
#
#	if test '$ac_silc_libs' != 'no'; then
#		SILC_LIBS='-L$ac_silc_libs'
#	fi
#	SILC_LIBS='$SILC_LIBS -lsilc -lsilcclient -lpthread $LIBDL'
#	AC_CHECK_LIB(silcclient, silc_client_init, [silcclient=yes], , $SILC_LIBS)
#
#	if test 'x$silcincludes' = 'xyes' -a 'x$silcclient' = 'xyes'; then
#		have_silc='yes'
#	fi
endif
#AC_SUBST(SILC_LIBS)
#AC_SUBST(SILC_CFLAGS)
# SILC Toolkit >= 1.0.1 has a new MIME API
#if test 'x$silcclient' = 'xyes'; then
#	conf.set('HAVE_SILCMIME_H', true)
#fi

#######################################################################
# Check for Gadu-Gadu protocol library (libgadu)
#######################################################################

libgadu = dependency('libgadu', version : '>= 1.12.0', required : false)
have_libgadu = libgadu.found()

if have_libgadu
	if not compiler.has_function('gg_is_gpl_compliant', args : '-lgadu')
#		LIBGADU_LIBS=''
#		LIBGADU_CFLAGS=''
		have_libgadu = false
		message('''
libgadu is not compatible with the GPL when compiled with OpenSSL support.

To compile against system libgadu, please recompile libgadu using:
./configure --with-openssl=no
Then rerun this ./configure

Falling back to using our own copy of libgadu.
		''')
	endif
endif

HAVE_LIBGADU = have_libgadu
if have_libgadu
	conf.set('HAVE_LIBGADU', true)
else
	gg_have_gnutls_csxst = compiler.has_function('gnutls_certificate_set_x509_system_trust', args : '-lgnutls')

	gg_gnutls_sts = ''
	if not gg_have_gnutls_csxst
		foreach i : '/etc/ssl/ca-bundle.pem /etc/ssl/certs/ca-certificates.crt /etc/pki/tls/cert.pem /usr/local/share/certs/ca-root-nss.crt /etc/ssl/cert.pem'.split()
			if gg_gnutls_sts == '' and run_command('test', '-e', i).returncode() == 0
				gg_gnutls_sts = i
			endif
		endforeach
	endif

	if gg_have_gnutls_csxst
		conf.set('HAVE_GNUTLS_CERTIFICATE_SET_X509_SYSTEM_TRUST', true)
	endif
	if gg_gnutls_sts != ''
		conf.set('GG_CONFIG_GNUTLS_SYSTEM_TRUST_STORE',
                 '"@0@"'.format(gg_gnutls_sts))
	endif

	# redundant - only here to stay compatible with libgadu upstream
	GNUTLS_2_10 = dependency('gnutls', version : '>= 2.10.0', required : false)
	conf.set('HAVE_GNUTLS_2_10', GNUTLS_2_10.found())
endif

#AC_SUBST(LIBGADU_LIBS)
#AC_SUBST(LIBGADU_CFLAGS)


DISTRIB = get_option('distrib')
DYNAMIC_PRPLS = ' '.join(get_option('dynamic-prpls').split(','))
STATIC_PRPLS = ' '.join(get_option('static-prpls').split(','))
if STATIC_PRPLS != '' and DYNAMIC_PRPLS == 'all'
	DYNAMIC_PRPLS = ''
endif

if STATIC_PRPLS == 'all'
	STATIC_PRPLS = 'bonjour facebook gg irc jabber msn mxit novell oscar sametime silc simple yahoo zephyr'
endif
#if not have_meanwhile
#	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/sametime//'`
#fi
if not avahiincludes or not avahilibs
#	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/bonjour//'`
endif
if not silcincludes or not silcclient
#	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/silc//'`
endif
if is_win32
#	STATIC_PRPLS=`echo $STATIC_PRPLS | $sedpath 's/zephyr//'`
endif
#AC_SUBST(STATIC_PRPLS)
STATIC_LINK_LIBS = []
extern_load = []
load_proto = []
extern_unload = []
unload_proto = []
foreach i : STATIC_PRPLS.split()
	# Ugly special case for 'libsilcpurple.la':
	if i == 'silc'
		STATIC_LINK_LIBS.append('\$(top_builddir)/libpurple/protocols/@1@/lib@1@purple.la'.format(meson.build_root(), i))
	else
		# FIXME: Shouldn't be libtool:
		STATIC_LINK_LIBS.append('\$(top_builddir)/libpurple/protocols/@1@/lib@1@.la'.format(meson.build_root(), i))
	endif
	extern_load.append('extern gboolean @0@_plugin_load();'.format(i))
	load_proto.append('@0@_plugin_load();'.format(i))
	extern_unload.append('extern gboolean @0@_plugin_unload();'.format(i))
	unload_proto.append('@0@_plugin_unload();'.format(i))
endforeach
STATIC_BONJOUR = STATIC_PRPLS.contains('bonjour')
STATIC_FACEBOOK = STATIC_PRPLS.contains('facebook')
STATIC_GG = STATIC_PRPLS.contains('gg')
STATIC_IRC = STATIC_PRPLS.contains('irc')
STATIC_JABBER = STATIC_PRPLS.contains('jabber')
STATIC_MSN = STATIC_PRPLS.contains('msn')
STATIC_MXIT = STATIC_PRPLS.contains('mxit')
STATIC_NOVELL = STATIC_PRPLS.contains('novell')
STATIC_OSCAR = STATIC_PRPLS.contains('oscar') or STATIC_PRPLS.contains('aim') or STATIC_PRPLS.contains('icq')
STATIC_SAMETIME = STATIC_PRPLS.contains('sametime') and have_meanwhile
STATIC_SILC = STATIC_PRPLS.contains('silc') and have_silc
STATIC_SIMPLE = STATIC_PRPLS.contains('simple')
STATIC_YAHOO = STATIC_PRPLS.contains('yahoo')
STATIC_ZEPHYR = STATIC_PRPLS.contains('zephyr')
#AC_SUBST(STATIC_LINK_LIBS)
conf.set('STATIC_PROTO_LOAD',
         ' '.join(extern_load) +
         ' static void static_proto_load(void) { ' + ' '.join(load_proto) + ' }')
conf.set('STATIC_PROTO_UNLOAD',
         ' '.join(extern_unload) +
         ' static void static_proto_unload(void) { ' + ' '.join(unload_proto) + ' }')

if DYNAMIC_PRPLS == 'all'
	DYNAMIC_PRPLS = 'bonjour facebook gg irc jabber msn mxit novell oscar sametime silc simple yahoo zephyr'
endif
#if not have_meanwhile
#	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/sametime//'`
#endif
if not avahiincludes or not avahilibs
#	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/bonjour//'`
endif
if not silcincludes or not silcclient
#	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/silc//'`
endif
if is_win32
#	DYNAMIC_PRPLS=`echo $DYNAMIC_PRPLS | $sedpath 's/zephyr//'`
endif
#AC_SUBST(DYNAMIC_PRPLS)

dynamic_bonjour = DYNAMIC_PRPLS.contains('bonjour')
dynamic_facebook = DYNAMIC_PRPLS.contains('facebook')
dynamic_gg  = DYNAMIC_PRPLS.contains('gg ')
dynamic_irc = DYNAMIC_PRPLS.contains('irc')
dynamic_jabber = DYNAMIC_PRPLS.contains('jabber')
dynamic_msn = DYNAMIC_PRPLS.contains('msn')
dynamic_mxit = DYNAMIC_PRPLS.contains('mxit')
dynamic_novell = DYNAMIC_PRPLS.contains('novell')
dynamic_null = DYNAMIC_PRPLS.contains('null')
dynamic_oscar = DYNAMIC_PRPLS.contains('oscar') or DYNAMIC_PRPLS.contains('aim') or DYNAMIC_PRPLS.contains('icq')
dynamic_sametime = DYNAMIC_PRPLS.contains('sametime')
dynamic_silc = DYNAMIC_PRPLS.contains('silc')
dynamic_simple = DYNAMIC_PRPLS.contains('simple')
dynamic_yahoo = DYNAMIC_PRPLS.contains('yahoo')
dynamic_zephyr = DYNAMIC_PRPLS.contains('zephyr')

conf.set('HAVE_SYS_UTSNAME_H',
         compiler.has_header('sys/utsname.h'))
conf.set('HAVE_UNAME',
         compiler.has_function('uname'))


DEBUG_CFLAGS = '-DPURPLE_DISABLE_DEPRECATED -DPIDGIN_DISABLE_DEPRECATED -DFINCH_DISABLE_DEPRECATED -DGNT_DISABLE_DEPRECATED'
if compiler.get_id() == 'gcc'
	# We enable -Wall later.
	# If it's set after the warning CFLAGS in the compiler invocation, it counteracts the -Wno... flags.
	# This leads to warnings we don't want.
#	CFLAGS=`echo $CFLAGS |$sedpath 's/-Wall//'`

	# ENABLE WARNINGS SUPPORTED BY THE VERSION OF GCC IN USE
	#
	# Future Possibilities
	#
	# Consider adding -Wbad-function-cast.
	#	This leads to spurious warnings using GPOINTER_TO_INT(), et al. directly on a function call.
	#		We'd need an intermediate variable.
	#
	foreach newflag : [
			'-Waggregate-return',
			'-Wcast-align',
			'-Wdeclaration-after-statement',
			'-Wendif-labels',
			'-Werror-implicit-function-declaration',
			'-Wextra -Wno-unused-parameter',
			'-Wformat-security',
			'-Werror=format-security',
			'-Winit-self',
			'-Wmissing-declarations',
			'-Wmissing-noreturn',
			'-Wmissing-prototypes',
			'-Wpointer-arith',
			'-Wfloat-equal',
			'-Wundef']
#		orig_CFLAGS='$CFLAGS'
#		CFLAGS='$CFLAGS $newflag'
#		AC_MSG_CHECKING(for $newflag option to gcc)
#		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])], [
#			AC_MSG_RESULT(yes)
#			CFLAGS='$orig_CFLAGS'
#			DEBUG_CFLAGS='$DEBUG_CFLAGS $newflag'
#		], [
#			AC_MSG_RESULT(no)
#			CFLAGS='$orig_CFLAGS'
#		])
	endforeach

	if get_option('fortify')
#		AC_MSG_CHECKING(for FORTIFY_SOURCE support)
#		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <features.h>]], [[
#			#if !(__GNUC_PREREQ (4, 1) \
#				|| (defined __GNUC_RH_RELEASE__ && __GNUC_PREREQ (4, 0)) \
#				|| (defined __GNUC_RH_RELEASE__ && __GNUC_PREREQ (3, 4) \
#					&& __GNUC_MINOR__ == 4 \
#					&& (__GNUC_PATCHLEVEL__ > 2 \
#						|| (__GNUC_PATCHLEVEL__ == 2 && __GNUC_RH_RELEASE__ >= 8))))
#			#error No FORTIFY_SOURCE support
#			#endif
#				return 0;
#		]])], [
#			AC_MSG_RESULT(yes)
#			DEBUG_CFLAGS='$DEBUG_CFLAGS -Wp,-D_FORTIFY_SOURCE=2'
#		], [
#			AC_MSG_RESULT(no)
#		])
	endif

#	DEBUG_CFLAGS='-Wall $DEBUG_CFLAGS'
#	CFLAGS='-g $CFLAGS'
endif
#DEBUG_CPPFLAGS=`echo '$DEBUG_CFLAGS' | $sedpath 's/-Wdeclaration-after-statement//' | $sedpath 's/-Wmissing-prototypes//' | $sedpath 's/-Waggregate-return//'`
#
if SUNCC
#        CFLAGS='$CFLAGS -features=extensions'
endif
#AC_SUBST(CFLAGS)

pidginpath = find_program('pidgin')

have_clang = compiler.get_id() == 'clang'

if have_clang
#	GLIB_LIBS=`echo $GLIB_LIBS | $sedpath 's/-pthread/-lpthread/'`
endif

if get_option('glib-errors-trace')
	if have_clang
		error('--enable-glib-errors-trace doesn\'t work with clang')
	endif
	conf.set('ENABLE_GLIBTRACE', true)
#	CFLAGS='$CFLAGS -rdynamic'
endif

#######################################################################
# Check for D-Bus libraries
#######################################################################

# dbus doesn't compile for win32 at the moment
enable_dbus = get_option('dbus') and is_not_win32

if enable_dbus
	dbus_binding_tool = find_program('dbus-binding-tool')
	enable_dbus = dbus_binding_tool.found()
endif

if enable_dbus
	dbus = dependency('dbus-1', version : '>= 0.60', required : force_deps)
	dbus_glib = dependency('dbus-glib-1', version : '>= 0.60', required : force_deps)
	enable_dbus = dbus.found() and dbus_glib.found()
else
	dbus = declare_dependency()
endif

#######################################################################
# Check for Unity and Messaging Menu
#######################################################################
enable_unity = get_option('unity-integration')
if enable_unity
	UNITY = [
		dependency('unity', version : '>= 6.8'),
		dependency('messaging-menu', version : '>= 12.10')
	]
	USES_MM_CHAT_SECTION = 'X-MessagingMenu-UsesChatSection=true'
#	AC_SUBST(UNITY_CFLAGS)
#	AC_SUBST(UNITY_LIBS)
#	AC_SUBST(USES_MM_CHAT_SECTION)
endif
ENABLE_UNITY = enable_unity

#######################################################################
# Check for Secret Service headers
#######################################################################

# disabled - see secretservice.c

#if get_option('secret-service')
#	PKG_CHECK_MODULES(SECRETSERVICE, [libsecret-1], [
#		AC_SUBST(SECRETSERVICE_CFLAGS)
#		AC_SUBST(SECRETSERVICE_LIBS)
#		AC_DEFINE(HAVE_SECRETSERVICE, 1, [Define if we have Secret Service.])
#	])
#endif

#AM_CONDITIONAL(ENABLE_SECRETSERVICE, test 'x$enable_secret_service' = 'xyes')
ENABLE_SECRETSERVICE = false

#######################################################################
# Check for GNOME Keyring headers
#######################################################################

enable_gnome_keyring = get_option('gnome-keyring') and is_not_win32

if enable_gnome_keyring
	gnome_keyring = dependency('gnome-keyring-1', required : force_deps)
	if gnome_keyring.found()
		conf.set('HAVE_GNOMEKEYRING', true)
	else
		enable_gnome_keyring = false
	endif
endif

ENABLE_GNOMEKEYRING = enable_gnome_keyring

#######################################################################
# Check for KWallet headers
#######################################################################

enable_kwallet = get_option('kwallet') and is_not_win32

if enable_kwallet
	# Ensure C++ compiler works
#	AC_CHECK_PROG(CXXTEST, [$CXX], [$CXX])
#	if test 'x$CXXTEST' = 'x'; then
#		enable_kwallet = false
#		if test 'x$force_deps' = 'xyes'; then
#			AC_MSG_ERROR([
#A C++ compiler was not found.
#Use -Dkwallet=false if you do not need KWallet support.
#])
#		fi
#	fi
endif

#AC_LANG_PUSH([C++])
#CPPFLAGS_save='$CPPFLAGS'
#LIBS_save='$LIBS'

if enable_kwallet
#	PKG_CHECK_MODULES(QT4, [QtCore], [
#		AC_SUBST(QT4_CFLAGS)
#		AC_SUBST(QT4_LIBS)
#	], [
#		AC_MSG_RESULT(no)
#		enable_kwallet = false
#		if test 'x$force_deps' = 'xyes'; then
#			AC_MSG_ERROR([
#Qt4 development headers not found.
#Use -Dkwallet=false if you do not need KWallet support.
#])
#		fi
#	])
endif

if enable_kwallet
#	AC_MSG_CHECKING([for metaobject compiler])
#	MOC=`$PKG_CONFIG --variable=moc_location QtCore`
#	AC_SUBST(MOC)
#	AC_MSG_RESULT([$MOC])
#
#	KWALLET_CXXFLAGS=''
#	KWALLET_LIBS=''
#	if test -z '$with_kwallet_includes' || test -z '$with_kwallet_libs'; then
#		AC_CHECK_PROG(KDE4_CONFIG, kde4-config, kde4-config, no)
#		if test 'x$KDE4_CONFIG' = 'xno'; then
#			enable_kwallet = false
#			if test 'x$force_deps' = 'xyes'; then
#				AC_MSG_ERROR([
#kde4-config not found. $KDE4_CONFIG
#Use -Dkwallet=false if you do not need KWallet support.
#Use --with-kwallet-includes and --with-kwallet-libs to set up includes manually.
#])
#			fi
#		fi
#	fi
endif

if enable_kwallet
#	if test '$ac_kwallet_includes' != 'no'; then
#		KWALLET_CXXFLAGS='-I$ac_kwallet_includes'
#	elif test 'x$KDE4_CONFIG' != 'xno'; then
#		KWALLET_CXXFLAGS='$QT4_CFLAGS -I`$KDE4_CONFIG --path include`'
#	fi
#	CPPFLAGS='$CPPFLAGS $KWALLET_CXXFLAGS'
#	AC_CHECK_HEADER([kwallet.h], , [
#		enable_kwallet = false
#		if test 'x$force_deps' = 'xyes'; then
#			AC_MSG_ERROR([
#KWallet development headers not found.
#Use -Dkwallet=false if you do not need KWallet support.
#])
#		fi
#])
endif

if enable_kwallet
#	AC_MSG_CHECKING([for KWallet libraries])
#	if test '$ac_kwallet_libs' != 'no'; then
#		KWALLET_LIBS='-L$ac_kwallet_libs -lkdeui'
#	elif test 'x$KDE4_CONFIG' != 'xno'; then
#		KWALLET_LIBS='-L`$KDE4_CONFIG --install lib`/kde4/devel -lkdeui'
#	else
#		KWALLET_LIBS='-lkdeui'
#	fi
#	KWALLET_LIBS='$KWALLET_LIBS'
#	LIBS='$LIBS $KWALLET_LIBS $QT4_LIBS'
#	AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <kwallet.h>],
#		[KWallet::Wallet::LocalWallet();])], [AC_MSG_RESULT([yes])],
#		[
#			AC_MSG_RESULT(no)
#			enable_kwallet = false
#			if test 'x$force_deps' = 'xyes'; then
#				AC_MSG_ERROR([
#KWallet development libraries not found.
#Use -Dkwallet=false if you do not need KWallet support.
#])
#			fi
#		])
endif

#CPPFLAGS='$CPPFLAGS_save'
#LIBS='$LIBS_save'
#AC_LANG_POP

#AC_SUBST(KWALLET_CXXFLAGS)
#AC_SUBST(KWALLET_LIBS)

ENABLE_KWALLET = enable_kwallet and false

#######################################################################
# Check for GPlugin 0.0.17
#######################################################################
if get_option('plugins')
	gplugin = dependency('gplugin', version : '>= 0.0.17')
	# GPLUGIN_REG sets pkg-config requirements in the .pc file
	conf.set('GPLUGIN_REQ', ', gplugin')
	gmodule = dependency('gmodule-2.0')
	enable_introspection = true
else
	enable_introspection = false
endif

if enable_introspection
	gnome = import('gnome')
	conf.set('ENABLE_INTROSPECTION', true)
endif

#######################################################################
# Check for Python
#######################################################################

# Python scripts are used to auto-generate about 3000 lines of C
# and XML code that wraps (part of) the existing API so that
# it is now accessible through D-Bus.

if enable_dbus or enable_consoleui
	python = find_program('python3')
endif

###########################################################################
# Find the D-Bus services dir.
#
# This is a 3 step process that
#
# 1. checks if --with-dbus-services was set, if so use that.
# 2. checks if --prefix was given, if so use that.
# 3. fallbacks to installing into what should be the correct system
#    directories.
#
# This is still prone to error if one of the legacy directories exist
# although a newer dbus is installed.  But I have tried to order the
# directory searching to keep this situation at a minimum.
###########################################################################

DBUS_SERVICES_DIR = ''
dbus_services = get_option('dbus-services')

if enable_dbus
#	AC_MSG_CHECKING([location of the D-Bus services directory])
	if dbus_services != ''
		if run_command('test', '-d', dbus_services).returncode() != 0
			error(dbus_services + ' does not exist, if this is the correct location please make sure that it exists.')
		endif

		DBUS_SERVICES_DIR = dbus_services
	else
		if get_option('prefix') != ''
			# no prefix given, so we look for the correct dbus system paths.
			# if a prefix is given, we use it.

			serviceprefixes = [get_option('prefix') + '/share', get_option('prefix') + '/lib', '/usr/share', '/usr/local/share']
			DBUS_SERVICES_DIR = ''

			foreach d : serviceprefixes
				dir = d + '/dbus-1/services'
				if DBUS_SERVICES_DIR == '' and run_command('test', '-d', dir).returncode() == 0
					DBUS_SERVICES_DIR = dir
				endif
			endforeach

			if DBUS_SERVICES_DIR == ''
				error('D-Bus services directory was not found!  Please use --with-dbus-services and specify its location.')
			endif
		else
			DBUS_SERVICES_DIR = get_option('datadir') + '/dbus-1/services'
		endif
	endif
#	AC_MSG_RESULT([$DBUS_SERVICES_DIR])
	conf.set('HAVE_DBUS', true)
endif
#AC_SUBST(DBUS_SERVICES_DIR)

if enable_dbus
	message('Building with D-Bus support')
else
	message('Building without D-Bus support')
endif

ENABLE_DBUS = enable_dbus

# Check for Python headers (currently useful only for libgnt)
# (Thanks to XChat)
#if test 'x$enable_consoleui' = 'xyes' -a ! -z '$PYTHON' -a x'$PYTHON' != x'no' ; then
#	AC_MSG_CHECKING(for Python compile flags)
#	if test -f ${PYTHON}-config; then
#		PY_CFLAGS=`${PYTHON}-config --includes`
#		PY_LIBS=`${PYTHON}-config --libs`
#		AC_DEFINE(USE_PYTHON, [1], [Define if python headers are available.])
#		AC_MSG_RESULT(ok)
#	else
#		AC_MSG_RESULT([Cannot find ${PYTHON}-config])
#		PY_CFLAGS=''
#		PY_LIBS=''
#	endif
#endif
#AC_SUBST(PY_CFLAGS)
#AC_SUBST(PY_LIBS)

#######################################################################
# SSL support
#######################################################################

ssl_certificates_dir = get_option('system-ssl-certs')

SSL_CERTIFICATES_DIR = ''
if ssl_certificates_dir != ''
	if run_command('test', '-d', ssl_certificates_dir).returncode() != 0
		if is_win32
			message(ssl_certificates_dir + ' does not exist. It may be OK when cross-compiling, but please make sure about it.')
		else
			error(ssl_certificates_dir + ' does not exist, if this is the correct location please make sure that it exists.')
		endif
	endif
	SSL_CERTIFICATES_DIR = ssl_certificates_dir
endif
#AC_SUBST(SSL_CERTIFICATES_DIR)
if SSL_CERTIFICATES_DIR != ''
	conf.set('SSL_CERTIFICATES_DIR',
	         '"@0@"'.format(SSL_CERTIFICATES_DIR))
endif
INSTALL_SSL_CERTIFICATES = SSL_CERTIFICATES_DIR == ''

# These two are inverses of each other <-- stolen from evolution!
enable_gnutls = get_option('gnutls')
have_gnutls = false
enable_nss = get_option('nss')
have_nss = false

msg_ssl = 'None. MSN, Yahoo!, Novell Groupwise and Google Talk will not work without GnuTLS or NSS. OpenSSL is NOT usable!'
msg_gnutls = ''
msg_nss = ''

#
# Check for GnuTLS if it isn't disabled
#
looked_for_gnutls = false
if enable_gnutls
	looked_for_gnutls = true

	# minimum required version should almost certainly be higher
	gnutls = dependency('gnutls', version : '>= 2.10', required : false)
	have_gnutls = gnutls.found()
	if have_gnutls
		conf.set('HAVE_GNUTLS', true)
		msg_gnutls = 'GnuTLS'
	endif
endif

USE_GNUTLS = have_gnutls

#
# Check for NSS if it isn't disabled
#
looked_for_nss = false
if enable_nss
	looked_for_nss = true

	# TODO: set required minimum version
	nss = dependency('mozilla-nss', required : false)
	if nss.found()
		have_nss = true
	else
		# TODO: set required minimum version
		nss = dependency('nss', required : false)
		have_nss = nss.found()
	endif

	if have_nss
		conf.set('HAVE_NSS', true)
		msg_nss = 'Mozilla NSS'
	else
		error('No SSL/TLS library available')
	endif
endif

USE_NSS = have_nss


if msg_nss != '' and msg_gnutls != ''
	msg_ssl = msg_nss + ' and ' + msg_gnutls
elif msg_nss != ''
	msg_ssl = msg_nss
elif msg_gnutls != ''
	msg_ssl = msg_gnutls
elif looked_for_gnutls and looked_for_nss and force_deps
	error('''
Neither GnuTLS or NSS SSL development headers found.
Use -Dnss=false -Dgnutls=false if you do not need SSL support.
MSN, Yahoo!, Novell Groupwise and Google Talk will not work without GnuTLS or NSS. OpenSSL is NOT usable!
''')
elif looked_for_gnutls and force_deps
	error('''
GnuTLS SSL development headers not found.
Use -Dgnutls=false if you do not need SSL support.
MSN, Yahoo!, Novell Groupwise and Google Talk will not work without SSL support.
''')
elif looked_for_nss and force_deps
	error('''
NSS SSL development headers not found.
Use -Dnss=false if you do not need SSL support.
MSN, Yahoo!, Novell Groupwise and Google Talk will not work without SSL support.
''')
endif

#if test '$ac_cv_cygwin' = yes ; then
#	LDADD='$LDADD -static'
#	conf.set('DEBUG', true)
#endif

#AC_SUBST(DEBUG_CPPFLAGS)
#AC_SUBST(DEBUG_CFLAGS)
#AC_SUBST(LDADD)
#AC_SUBST(LIBS)

if get_option('plugins')
	conf.set('PURPLE_PLUGINS', 1)
	PLUGINS = true
	PLUGINS_DEFINE = '#define PURPLE_PLUGINS 1'
else
	PLUGINS = false
	PLUGINS_DEFINE = '#undef PURPLE_PLUGINS'
endif

#######################################################################
# Check for Cyrus-SASL (for xmpp/irc)
#######################################################################
foreach func : ['snprintf', 'connect']
	conf.set('HAVE_' + func.to_upper(),
	         compiler.has_function(func))
endforeach
#AC_SUBST(SASL_LIBS)
SASL_LIBS = ''
enable_cyrus_sasl = get_option('cyrus-sasl')
if enable_cyrus_sasl
	if compiler.has_function('sasl_client_init', args : '-lsasl2')
		USE_CYRUS_SASL = true
		conf.set('HAVE_CYRUS_SASL', true)
		SASL_LIBS = '-lsasl2'
	else
		USE_CYRUS_SASL = false
		error('Cyrus SASL library not found')
	endif
else
	USE_CYRUS_SASL = false
endif

#######################################################################
# Check for Kerberos (for Zephyr)
#######################################################################
conf.set('ZEPHYR_INT32', 'long')
#AC_SUBST(KRB4_CFLAGS)
#AC_SUBST(KRB4_LDFLAGS)
#AC_SUBST(KRB4_LIBS)
kerberos = get_option('krb4')
if kerberos
	if kerberos != 'yes'
#		KRB4_CFLAGS='-I${kerberos}/include'
#		if test -d '$kerberos/include/kerberosIV' ; then
#			KRB4_CFLAGS='$KRB4_CFLAGS -I${kerberos}/include/kerberosIV'
#		fi
#		KRB4_LDFLAGS='-L${kerberos}/lib'
	elif run_command('test', '-d', '/usr/local/include/kerberosIV').returncode() == 0
#		KRB4_CFLAGS='-I/usr/local/include/kerberosIV'
	elif run_command('test', '-d', '/usr/include/kerberosIV').returncode() == 0
#		KRB4_CFLAGS='-I/usr/include/kerberosIV'
	endif
	conf.set('ZEPHYR_USES_KERBEROS', true)

#	orig_LDFLAGS='$LDFLAGS'
#	LDFLAGS='$LDFLAGS $KRB4_LDFLAGS'
#	AC_CHECK_LIB(krb4, krb_rd_req,
#			[KRB4_LIBS='-lkrb4 -ldes425 -lkrb5 -lk5crypto -lcom_err'],
#			[AC_CHECK_LIB(krb, krb_rd_req,
#				[KRB4_LIBS='-lkrb -ldes'],
#				[AC_MSG_ERROR([Kerberos 4 libraries not found])],
#				-ldes)],
#			-ldes425 -lkrb5 -lk5crypto -lcom_err)
#	orig_LIBS='$LIBS'
#	LIBS='$LIBS $KRB4_LIBS'
#	AC_CHECK_FUNCS(krb_set_key krb_rd_req krb_get_lrealm)
#	AC_CHECK_FUNCS(krb_get_err_text krb_log)
#	LIBS='$orig_LIBS'
#	LDFLAGS='$orig_LDFLAGS'
endif

#######################################################################
# Check for external libzephyr
#######################################################################
zephyr = get_option('zephyr')
EXTERNAL_LIBZEPHYR = zephyr
#AC_SUBST(ZEPHYR_CFLAGS)
#AC_SUBST(ZEPHYR_LDFLAGS)
#AC_SUBST(ZEPHYR_LIBS)
if zephyr
#	if test '$zephyr' != 'yes' ; then
#		ZEPHYR_CFLAGS='-I${zephyr}/include'
#		ZEPHYR_LDFLAGS='-L${zephyr}/lib'
#	elif test -d /usr/athena/include/zephyr ; then
#		ZEPHYR_CFLAGS='-I/usr/athena/include'
#	elif test -d /usr/include/zephyr ; then
#		ZEPHYR_CFLAGS='-I/usr/include'
#	elif test -d /usr/local/include/zephyr ; then
#		ZEPHYR_CFLAGS='-I/usr/local/include'
#	fi
	conf.set('LIBZEPHYR_EXT', true)
	EXTERNAL_LIBZEPHYR = zephyr
#	orig_LDFLAGS='$LDFLAGS'
#	LDFLAGS='$LDFLAGS $ZEPHYR_LDFLAGS'
#	AC_CHECK_LIB(zephyr, ZInitialize,
#		[ZEPHYR_LIBS='-lzephyr'],
#		[AC_MSG_ERROR([Zephyr libraries not found])],
#		-lzephyr)
#	orig_LIBS='$LIBS'
#	LIBS='$orig_LIBS'
#	LDFLAGS='$orig_LDFLAGS'
endif

#AC_MSG_CHECKING(for me pot o' gold)
#AC_MSG_RESULT(no)
foreach func : 'gethostid lrand48 timegm memcpy memmove random strchr strerror vprintf'.split()
	conf.set('HAVE_' + func.to_upper(),
	         compiler.has_function(func))
endforeach
foreach header : 'malloc.h paths.h sgtty.h stdarg.h sys/cdefs.h sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h sys/select.h sys/uio.h sys/utsname.h sys/wait.h termios.h'.split()
	conf.set('HAVE_' + header.to_upper().underscorify(),
	         compiler.has_header(header))
endforeach

# sys/sysctl.h on OpenBSD 4.2 requires sys/param.h
# sys/sysctl.h on FreeBSD requires sys/types.h
conf.set('HAVE_SYS_PARAM_H',
         compiler.has_header('sys/param.h'))
#AC_CHECK_HEADERS(sys/sysctl.h, [], [],
#	[[
#		#include <sys/types.h>
#		#ifdef HAVE_PARAM_H
#		# include <sys/param.h>
#		#endif
#	]])
#
conf.set('HAVE_SYS_SOCKET_H',
         compiler.has_header('sys/socket.h'))
#AC_VAR_TIMEZONE_EXTERNALS

conf.set('HAVE_TM_GMTOFF',
         compiler.has_member('struct tm', 'tm_gmtoff',
                             prefix : '#include<time.h>'))

#######################################################################
# Disable pixmap installation
#######################################################################
INSTALL_PIXMAPS = get_option('pixmaps-install')

#######################################################################
# Tweak status tray icon installation directory
#######################################################################
ENABLE_TRAYCOMPAT = get_option('trayicon-compat')

# check for gtk-doc
#m4_ifdef([GTK_DOC_CHECK], [
#GTK_DOC_CHECK([1.16],[--flavour no-tmpl])
#],[
#	if test 'x$force_deps' = 'xyes' -a 'x$enable_gtk_doc' = 'xyes'; then
#		AC_MSG_ERROR([
#You have requested to generate documentation, but gtk-doc was not found.
#		])
#	fi
#
#	AM_CONDITIONAL([ENABLE_GTK_DOC], false)
#	enable_gtk_doc=no
#])
ENABLE_GTK_DOC = false

enable_debug = get_option('debug')
conf.set('DEBUG', enable_debug)

PURPLE_AVAILABLE = true

# So that config.h may be found.
toplevel_inc = include_directories('.')

#subdir('doc')
#subdir('doc/reference')
#subdir('doc/reference/finch')
#subdir('doc/reference/libpurple')
#subdir('doc/reference/protocols')
#subdir('doc/reference/protocols/facebook')
#subdir('doc/reference/pidgin')
#subdir('pidgin')
#subdir('pidgin/pixmaps')
#subdir('pidgin/pixmaps/emotes/default/24')
#subdir('pidgin/pixmaps/emotes/small/16')
#subdir('pidgin/plugins')
#subdir('pidgin/plugins/cap')
#subdir('pidgin/plugins/disco')
#subdir('pidgin/plugins/gestures')
#subdir('pidgin/plugins/gevolution')
#subdir('pidgin/plugins/musicmessaging')
#subdir('pidgin/plugins/ticker')
#subdir('pidgin/plugins/win32/transparency')
#subdir('pidgin/plugins/win32/winprefs')
#subdir('pidgin/themes')
#subdir('libpurple/data/gconf')
#subdir('libpurple/example')
#subdir('libpurple/plugins')
#subdir('libpurple/plugins/keyrings')
subdir('libpurple')
#subdir('libpurple/protocols')
#subdir('libpurple/protocols/bonjour')
#subdir('libpurple/protocols/facebook')
#subdir('libpurple/protocols/gg')
#subdir('libpurple/protocols/irc')
#subdir('libpurple/protocols/jabber')
#subdir('libpurple/protocols/jabber/tests')
#subdir('libpurple/protocols/msn')
#subdir('libpurple/protocols/mxit')
#subdir('libpurple/protocols/novell')
#subdir('libpurple/protocols/null')
#subdir('libpurple/protocols/oscar')
#subdir('libpurple/protocols/oscar/tests')
#subdir('libpurple/protocols/sametime')
#subdir('libpurple/protocols/silc')
#subdir('libpurple/protocols/simple')
#subdir('libpurple/protocols/yahoo')
#subdir('libpurple/protocols/yahoo/tests')
#subdir('libpurple/protocols/zephyr')
#subdir('libpurple/tests')
#subdir('share/sounds')
#subdir('share/ca-certs')
#subdir('finch')
#subdir('finch/libgnt')
#subdir('finch/libgnt/wms')
#subdir('finch/plugins')

#		   doc/pidgin.1
#		   doc/finch.1
#		   doc/reference/finch/version.xml
#		   doc/reference/libpurple/version.xml
#		   doc/reference/protocols/version.xml
#		   doc/reference/pidgin/version.xml
#		   pidgin/data/pidgin.desktop.in
#		   pidgin/data/pidgin-3.pc
#		   pidgin/data/pidgin-3-uninstalled.pc
#		   pidgin/win32/pidgin_dll_rc.rc
#		   pidgin/win32/pidgin_exe_rc.rc
#		   libpurple/data/purple-3.pc
#		   libpurple/data/purple-3-uninstalled.pc
#		   libpurple/win32/libpurplerc.rc
#		   finch/finch.pc
#		   finch/finch_winres.rc
#		   finch/libfinch_winres.rc
#		   finch/libgnt/gnt.pc
#		   finch/libgnt/libgnt_winres.rc
#		   po/Makefile.in
#		  ])
configure_file(output : 'config.h',
               configuration : conf)

message('')
message('pidgin ' + purple_display_version)

message('')
message('Build GTK+ UI................. : ' + get_option('gtkui').to_string())
message('Build console UI.............. : ' + enable_consoleui.to_string())
message('Build for X11................. : ' + with_x.to_string())
message('')
message('Enable Gestures............... : ' + enable_gestures.to_string())
message('Protocols to build dynamically : ' + DYNAMIC_PRPLS)
message('Protocols to link statically.. : ' + STATIC_PRPLS)
message('')
message('Build with GStreamer support.. : ' + enable_gst.to_string())
message('Build with D-Bus support...... : ' + enable_dbus.to_string())
message('Build with voice and video.... : ' + enable_vv.to_string())
if enable_dbus
	message('D-Bus services directory...... :' + DBUS_SERVICES_DIR)
endif
message('Build with GNU Libidn......... : ' + get_option('idn').to_string())
message('SSL Library/Libraries......... : ' + msg_ssl)
if SSL_CERTIFICATES_DIR != ''
	message('SSL CA certificates directory. : ' + SSL_CERTIFICATES_DIR)
endif
message('Build with Cyrus SASL support. : ' + enable_cyrus_sasl.to_string())
message('Use kerberos 4 with zephyr.... : ' + kerberos.to_string())
message('Use external libzephyr........ : ' + zephyr.to_string())
message('Use external libgadu.......... : ' + have_libgadu.to_string())
message('Install pixmaps............... : ' + INSTALL_PIXMAPS.to_string())
message('Old tray icon compatibility... : ' + ENABLE_TRAYCOMPAT.to_string())
message('Install translations.......... : ' + INSTALL_I18N.to_string())
message('Has you....................... : yes')
message('')
message('Use XScreenSaver Extension.... : ' + enable_screensaver.to_string())
message('Use X Session Management...... : ' + enable_sm.to_string())
message('Use startup notification...... : ' + enable_startup_notification.to_string())
message('Build with Enchant support.... : ' + use_enchant.to_string())
message('Build with GCR widgets........ : ' + enable_gcr.to_string())
message('Build Unity integration plugin.: ' + enable_unity.to_string())
message('')
message('Build with GNOME Keyring...... : ' + enable_gnome_keyring.to_string())
message('Build with KWallet............ : ' + enable_kwallet.to_string())
#message('Build with Secret Service..... : ' + enable_secret_service)
message('')
message('Build with plugin support..... : ' + PLUGINS.to_string())
message('Enable Introspection...........: ' + enable_introspection.to_string())

if is_win32
	message('')
	message('Win32 directory structure..... : ' + get_option('win32-dirs'))
endif

message('')
message('Print debugging messages...... : ' + enable_debug.to_string())
message('Generate documentation........ : ' + ENABLE_GTK_DOC.to_string())
message('')
message('Pidgin will be installed in @0@.'.format(get_option('bindir')))
if pidginpath.found()
	message('Warning: You have an old copy of Pidgin at @0@.'.format(pidginpath.path()))
endif
if not INSTALL_PIXMAPS
	message('')
	message('Warning: You have disabled the installation of pixmap data, but Pidgin')
	message('still requires installed pixmaps.  Be sure you know what you are doing.')
endif
if not INSTALL_I18N
	message('')
	message('Warning: You have disabled the building and installation of translation')
	message('data.  This will prevent building pidgin.desktop and the GConf schemas.')
	message('Be sure you know what you are doing.')
endif
message('')
message('configure complete, now type \'ninja\'')
message('')

