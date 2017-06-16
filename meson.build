project('pidgin', 'c', meson_version : '>0.36.0')

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

conf.set('PACKAGE', '"@0@"'.format(meson.project_name()))
conf.set('PACKAGE_NAME', '"@0@"'.format(meson.project_name()))
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

package_revision = vcs_tag(
    input : 'package_revision.h.in',
    output : 'package_revision.h',
    fallback : meson.project_version())

sedpath = find_program('sed')

# Storing configure arguments
# conf.set('CONFIG_ARGS', ac_configure_args)

# Checks for programs.
compiler = meson.get_compiler('c')

if compiler.has_function('alloca')  # FIXME: Probably not enough.
	conf.set('HAVE_ALLOCA_H', true)
else
	error('alloca could not be found')
endif

# Check for Sun compiler
SUNCC = compiler.compiles('void main() {__SUNPRO_C;};')

# Check for Win32
if host_machine.system() == 'windows'
	windows = import('windows')

	is_win32 = true
	is_not_win32 = false
	# FIXME: Need to add ws2_32 to some target.
	ws2_32 = compiler.find_library('ws2_32')
	dnsapi = compiler.find_library('dnsapi')
	conf.set('IS_WIN32_CROSS_COMPILED', true)
	conf.set('WIN32_LEAN_AND_MEAN', true)
else
	is_win32 = false
	is_not_win32 = true
	ws2_32 = []
	dnsapi = []
endif
IS_WIN32 = is_win32

# Checks for header files.
# AC_HEADER_SYS_WAIT:
conf.set('HAVE_SYS_WAIT_H', compiler.has_header('sys/wait.h'))

foreach h : ['fcntl.h', 'unistd.h', 'stdint.h']
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

conf.set('WORDS_BIGENDIAN', host_machine.endian() != 'little')

conf.set('USE_WIN32_FHS',
    is_win32 and get_option('win32-dirs') == 'fhs')

# Check for directories
if is_win32
	if get_option('win32-dirs') == 'fhs'
		foreach dir : ['bin', 'lib', 'data', 'sysconf', 'locale']
			path = get_option(dir + 'dir')
			if not path.startswith('/')
				path = get_option('prefix') + '/' + path
			endif
			conf.set('WIN32_FHS_@0@DIR'.format(dir.to_upper()), path)
		endforeach

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
	foreach dir : ['data', 'sysconf', 'locale']
		path = get_option(dir + 'dir')
		if not path.startswith('/')
			path = get_option('prefix') + '/' + path
		endif
		set_variable('purple_@0@dir'.format(dir), '"@0@"'.format(path))
	endforeach

	common_libdir = get_option('libdir')
	if not common_libdir.startswith('/')
		common_libdir = get_option('prefix') + '/' + common_libdir
	endif
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

abslibdir = get_option('libdir')
if not abslibdir.startswith('/')
	abslibdir = get_option('prefix') + '/' + abslibdir
endif
PURPLE_PLUGINDIR = '@0@/purple-@1@'.format(abslibdir, purple_major_version)
conf.set('PURPLE_PLUGINDIR', '"@0@"'.format(PURPLE_PLUGINDIR))
PIDGIN_PLUGINDIR = '@0@/pidgin-@1@'.format(abslibdir, purple_major_version)
conf.set('PIDGIN_PLUGINDIR', '"@0@"'.format(PIDGIN_PLUGINDIR))
FINCH_PLUGINDIR = '@0@/finch-@1@'.format(abslibdir, purple_major_version)
conf.set('FINCH_PLUGINDIR', '"@0@"'.format(FINCH_PLUGINDIR))

# Checks for library functions.
foreach func : ['strdup']
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
if is_not_win32
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
	conf.set('HAVE_INET_NTOP',
	    compiler.has_function('inet_ntop'))
endif
conf.set('HAVE_GETIFADDRS',
    compiler.has_function('getifaddrs'))

# Check for socklen_t (in Unix98)
if is_win32
	socket_header = 'ws2tcpip.h'
else
	socket_header = 'sys/socket.h'
endif
if not compiler.has_header_symbol(socket_header, 'socklen_t')
	code = '''
#include <sys/types.h>
#include <@0@>
int accept(int, struct sockaddr *, size_t *);
int main() {}
'''.format(socket_header)
	if compiler.compiles(code, name : 'socklen_t is size_t')
		conf.set('socklen_t', 'size_t')
	else
		conf.set('socklen_t', 'int')
	endif
endif

# Some systems do not have sa_len field for struct sockaddr.
conf.set('HAVE_STRUCT_SOCKADDR_SA_LEN',
    compiler.has_member('struct sockaddr', 'sa_len',
        prefix : '#include <@0@>'.format(socket_header)))

# Check for v6-only sockets
if is_win32
	header = 'ws2tcpip.h'
else
	header = 'netinet/in.h'
endif
conf.set('HAVE_IPV6_V6ONLY',
    compiler.has_header_symbol(header, 'IPV6_V6ONLY'))

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

	intltool_merge = find_program('intltool-merge')
endif

# #######################################################################
# # Check for GLib 2.34 (required)
# #######################################################################
glib = dependency('glib-2.0', version : '>= 2.23.0')
gio = dependency('gio-2.0')
gmodule = dependency('gmodule-2.0')
gobject = dependency('gobject-2.0')
gthread = dependency('gthread-2.0')
gnome = import('gnome')

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
			with_x = false
			if force_deps
				error('''
X11 development headers not found.
Use -Dx=false if you do not need X11 support.
''')
			endif
		endif
	else
		enable_screensaver = false
		enable_sm = false
		enable_gestures = false
		x11 = []
	endif

	#######################################################################
	# Check for XScreenSaver
	#######################################################################
	xss = []
	if enable_screensaver
		if with_x
			xss_headers = []
			xss_libs = []
			if compiler.has_function('XScreenSaverRegister',
			    args : ['-lXext', '-lX11', '-lXext', '-lm'])
				xss_libs = ['-lXext', '-lX11', '-lXext', '-lm']
			elif compiler.has_function('XScreenSaverRegister',
			    args : ['-lXss', '-lX11', '-lXext', '-lm'])
				xss_libs = ['-lXss', '-lX11', '-lXext', '-lm']
			endif
			if xss_libs != []
				if not compiler.has_header('X11/extensions/scrnsaver.h',
				    prefix : '#include <X11/Xlib.h>')
					enable_screensaver = false
				endif
			else
				enable_screensaver = false
			endif

			if enable_screensaver
				conf.set('USE_SCREENSAVER', true)
				xss = declare_dependency(
				    link_args : xss_libs
				)
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
			found_sm_lib = compiler.has_function('SmcSaveYourselfDone',
			    args : '-lSM')
			if found_sm_lib
				if compiler.has_header('X11/SM/SMlib.h')
					sm = declare_dependency(
					    link_args : ['-lSM', '-lICE']
					)
					enable_sm = true
				endif
			endif

			if enable_sm
				conf.set('USE_SM', true)
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
	if not enable_sm
		sm = []
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
ncurses_inc = []
ncurses_libs = []
enable_consoleui = get_option('consoleui')
force_finch = enable_consoleui
if enable_consoleui
	ncurses_libs = [
		compiler.find_library('ncursesw', required : false),
		compiler.find_library('panelw', required : false)
	]
	if not ncurses_libs[0].found() or not ncurses_libs[1].found()
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
		foreach location : ncurses_sys_dirs
			f = location + '/ncurses.h'
			if not found_ncurses_h
				if compiler.has_header_symbol(f, 'get_wch',
				       prefix : '#define _XOPEN_SOURCE_EXTENDED')
					if location != '.'
						ncurses_inc += [include_directories(location)]
					endif
					found_ncurses_h = true
				endif
			endif
		endforeach

		if not found_ncurses_h
			ncurses_inc = []
			ncurses_libs = []
			enable_consoleui = false
		endif
	else
		# ncursesw was not found. Look for plain old ncurses
		ncurses_libs = [
			compiler.find_library('ncurses', required : false),
			compiler.find_library('panel', required : false)
		]
		enable_consoleui = ncurses_libs[0].found() and ncurses_libs[1].found()
		conf.set('NO_WIDECHAR', true)
	endif
endif
ncurses = declare_dependency(
    include_directories : ncurses_inc,
    dependencies : ncurses_libs
)

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
if libxml.version().version_compare('<2.6.18')
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
else
	enable_vv = false
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
else
	gstreamer_app = []
endif

#######################################################################
# Check for Internationalized Domain Name support
#######################################################################

if get_option('idn')
	idn = dependency('libidn', version : '>= 0.0.0', required : force_deps)
	conf.set('USE_IDN', idn.found())
else
	idn = []
endif

#######################################################################
# Check for Meanwhile headers (for Sametime)
#######################################################################
if get_option('meanwhile')
	meanwhile = dependency('meanwhile', version : ['>= 1.0.0', '< 2.0.0'], required : force_deps)
	enable_meanwhile = meanwhile.found()
else
	enable_meanwhile = false
endif
#AC_SUBST(MEANWHILE_CFLAGS)
#AC_SUBST(MEANWHILE_LIBS)

#######################################################################
# Check for Native Avahi headers (for Bonjour)
#######################################################################

enable_avahi = get_option('avahi')
avahi = []
if enable_avahi and is_win32
	# Just keep enabled.
elif enable_avahi
	# Attempt to autodetect Avahi
	avahi_client = dependency('avahi-client', required : false)
	avahi_glib = dependency('avahi-glib', required : false)
	if avahi_client.found() and avahi_glib.found()
		avahi = [avahi_client, avahi_glib]
	else
		enable_avahi = false
		if force_deps
			error('''
avahi development package not found.
Use -Davahi=false if you do not need avahi (Bonjour) support.
''')
		endif
	endif
endif


#######################################################################
# Check for SILC client includes and libraries
#######################################################################
have_silc = false
if get_option('silc')
	silc = dependency('silcclient', version : '>= 1.1', required : false)
	if silc.found()
		have_silc = true
		# SILC Toolkit >= 1.0.1 has a new MIME API
		conf.set('HAVE_SILCMIME_H', true)
	else
		if force_deps
			error('''
SILC development package not found.
Use -Dsilc=false if you do not need SILC support.
''')
		endif
	endif
endif

#######################################################################
# Check for Gadu-Gadu protocol library (libgadu)
#######################################################################

enable_libgadu = get_option('libgadu')
if enable_libgadu
	libgadu = dependency('libgadu', version : '>= 1.12.0', required : false)
	have_libgadu = libgadu.found()

	if have_libgadu
		if not compiler.has_function('gg_is_gpl_compliant', dependencies : libgadu)
			have_libgadu = false
			message('''
libgadu is not compatible with the GPL when compiled with OpenSSL support.

To link against libgadu, please recompile libgadu using:
./configure --with-openssl=no
Then rerun this ./configure
			''')
		endif
	endif

	if not have_libgadu and force_deps
		error('''
Libgadu development headers not found.
Use -Dlibgadu=false if you do not need GG (GaduGadu) support.
''')
	endif
else
	have_libgadu = false
endif


DEFAULT_PRPLS = ['bonjour', 'facebook', 'gg', 'irc', 'jabber', 'novell',
                 'oscar', 'sametime', 'silc', 'simple', 'zephyr']
ALL_PRPLS = DEFAULT_PRPLS + ['null']

DISTRIB = get_option('distrib')
dynamic_list = get_option('dynamic-prpls').split(',')
static_list = get_option('static-prpls').split(',')
if (static_list != [''] and static_list != []) and dynamic_list == ['all']
	dynamic_list = []
endif

if static_list == ['all']
	static_list = DEFAULT_PRPLS
endif
STATIC_PRPLS = []
foreach prpl : static_list
	if prpl == ''
		# The list was empty; do nothing.
	elif prpl == 'sametime' and not enable_meanwhile
		# Do nothing
	elif prpl == 'bonjour' and not enable_avahi
		# Do nothing
	elif prpl == 'silc' and not have_silc
		# Do nothing
	elif prpl == 'gg' and not have_libgadu
		# Do nothing
	elif prpl == 'zephyr' and is_win32
		# Do nothing
	else
		STATIC_PRPLS += [prpl]
	endif
endforeach
STATIC_LINK_LIBS = []
extern_load = []
load_proto = []
extern_unload = []
unload_proto = []
foreach prpl : STATIC_PRPLS
	# Ugly special case for 'libsilcpurple.la':
	if prpl == 'silc'
		STATIC_LINK_LIBS.append('\$(top_builddir)/libpurple/protocols/@1@/lib@1@purple.la'.format(meson.build_root(), prpl))
	else
		# FIXME: Shouldn't be libtool:
		STATIC_LINK_LIBS.append('\$(top_builddir)/libpurple/protocols/@1@/lib@1@.la'.format(meson.build_root(), prpl))
	endif
	extern_load.append('extern gboolean @0@_plugin_load();'.format(prpl))
	load_proto.append('@0@_plugin_load();'.format(prpl))
	extern_unload.append('extern gboolean @0@_plugin_unload();'.format(prpl))
	unload_proto.append('@0@_plugin_unload();'.format(prpl))
endforeach
STATIC_BONJOUR = STATIC_PRPLS.contains('bonjour')
STATIC_FACEBOOK = STATIC_PRPLS.contains('facebook')
STATIC_GG = STATIC_PRPLS.contains('gg')
STATIC_IRC = STATIC_PRPLS.contains('irc')
STATIC_JABBER = STATIC_PRPLS.contains('jabber')
STATIC_NOVELL = STATIC_PRPLS.contains('novell')
STATIC_OSCAR = STATIC_PRPLS.contains('oscar') or STATIC_PRPLS.contains('aim') or STATIC_PRPLS.contains('icq')
STATIC_SAMETIME = STATIC_PRPLS.contains('sametime')
STATIC_SILC = STATIC_PRPLS.contains('silc')
STATIC_SIMPLE = STATIC_PRPLS.contains('simple')
STATIC_ZEPHYR = STATIC_PRPLS.contains('zephyr')
conf.set('STATIC_PROTO_LOAD',
    ' '.join(extern_load) +
    ' static void static_proto_load(void) { ' + ' '.join(load_proto) + ' }')
conf.set('STATIC_PROTO_UNLOAD',
    ' '.join(extern_unload) +
    ' static void static_proto_unload(void) { ' + ' '.join(unload_proto) + ' }')

if dynamic_list == ['all']
	dynamic_list = DEFAULT_PRPLS
endif
DYNAMIC_PRPLS = []
foreach prpl : dynamic_list
	if prpl == ''
		# The list was empty; do nothing.
	elif prpl == 'sametime' and not enable_meanwhile
		# Do nothing.
	elif prpl == 'bonjour' and not enable_avahi
		# Do nothing.
	elif prpl == 'silc' and not have_silc
		# Do nothing.
	elif prpl == 'gg' and not have_libgadu
		# Do nothing.
	elif prpl == 'zephyr' and is_win32
		# Do nothing.
	else
		DYNAMIC_PRPLS += [prpl]
	endif
endforeach

DYNAMIC_BONJOUR = DYNAMIC_PRPLS.contains('bonjour')
DYNAMIC_FACEBOOK = DYNAMIC_PRPLS.contains('facebook')
DYNAMIC_GG  = DYNAMIC_PRPLS.contains('gg ')
DYNAMIC_IRC = DYNAMIC_PRPLS.contains('irc')
DYNAMIC_JABBER = DYNAMIC_PRPLS.contains('jabber')
DYNAMIC_NOVELL = DYNAMIC_PRPLS.contains('novell')
DYNAMIC_NULL = DYNAMIC_PRPLS.contains('null')
DYNAMIC_OSCAR = DYNAMIC_PRPLS.contains('oscar') or DYNAMIC_PRPLS.contains('aim') or DYNAMIC_PRPLS.contains('icq')
DYNAMIC_SAMETIME = DYNAMIC_PRPLS.contains('sametime')
DYNAMIC_SILC = DYNAMIC_PRPLS.contains('silc')
DYNAMIC_SIMPLE = DYNAMIC_PRPLS.contains('simple')
DYNAMIC_ZEPHYR = DYNAMIC_PRPLS.contains('zephyr')

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
		if compiler.has_argument(newflag)
			add_global_arguments(newflag, language : 'c')
		endif
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
	add_global_arguments('-features=extensions', language : 'c')
endif

pidginpath = find_program('pidgin')

have_clang = compiler.get_id() == 'clang'

if get_option('glib-errors-trace')
	if have_clang
		error('--enable-glib-errors-trace doesn\'t work with clang')
	endif
	conf.set('ENABLE_GLIBTRACE', true)
	add_global_arguments('-rdynamic', language : 'c')
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
	dbus = []
	dbus_glib = []
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

enable_secret_service = get_option('secret-service')
if enable_secret_service
	secretservice = dependency('libsecret-1', required : force_deps)
	if secretservice.found()
	else
		enable_secret_service = false
	endif
endif

ENABLE_SECRETSERVICE = enable_secret_service

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
	add_languages('cpp')
	cxx_compiler = meson.get_compiler('cpp')

	qt4 = dependency('qt4', modules : 'Core', required : force_deps)
	enable_kwallet = qt4.found()
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
#])
#			fi
#		fi
#	fi
endif

if enable_kwallet
#	if test 'x$KDE4_CONFIG' != 'xno'; then
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
#	if test 'x$KDE4_CONFIG' != 'xno'; then
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
	enable_introspection = true
else
	gplugin = []
	enable_introspection = false
endif

if enable_introspection
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
if enable_consoleui
	python_dep = dependency('python')
endif
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

msg_ssl = 'None. Novell Groupwise and Google Talk will not work without GnuTLS or NSS. OpenSSL is NOT usable!'
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
Novell Groupwise and Google Talk will not work without GnuTLS or NSS. OpenSSL is NOT usable!
''')
elif looked_for_gnutls and force_deps
	error('''
GnuTLS SSL development headers not found.
Use -Dgnutls=false if you do not need SSL support.
Novell Groupwise and Google Talk will not work without SSL support.
''')
elif looked_for_nss and force_deps
	error('''
NSS SSL development headers not found.
Use -Dnss=false if you do not need SSL support.
Novell Groupwise and Google Talk will not work without SSL support.
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
		sasl = declare_dependency(link_args : '-lsasl2')
	else
		USE_CYRUS_SASL = false
		error('Cyrus SASL library not found')
	endif
else
	USE_CYRUS_SASL = false
endif
if not USE_CYRUS_SASL
	sasl = []
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
	krb4 = []
endif
if not kerberos
	krb4 = []
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
	ext_zephyr = []
endif
if not zephyr
	ext_zephyr = []
endif

#AC_MSG_CHECKING(for me pot o' gold)
#AC_MSG_RESULT(no)
foreach func : ['gethostid', 'timegm']
	conf.set('HAVE_' + func.to_upper(),
	    compiler.has_function(func))
endforeach
foreach header : 'paths.h sgtty.h sys/cdefs.h sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h sys/select.h sys/uio.h sys/utsname.h sys/wait.h termios.h'.split()
	conf.set('HAVE_' + header.to_upper().underscorify(),
	    compiler.has_header(header))
endforeach

# sys/sysctl.h on OpenBSD 4.2 requires sys/param.h
# sys/sysctl.h on FreeBSD requires sys/types.h
have_sys_param_h = compiler.has_header('sys/param.h')
conf.set('HAVE_SYS_PARAM_H', have_sys_param_h)
prefix = '''
#include <sys/types.h>
'''
if have_sys_param_h
	prefix += '''
#include <sys/param.h>
'''
endif
conf.set('HAVE_SYS_SYSCTL_H',
    compiler.has_header('sys/sysctl.h', prefix : prefix))
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

subdir('doc')
#subdir('doc/reference')
#subdir('doc/reference/finch')
#subdir('doc/reference/libpurple')
#subdir('doc/reference/protocols')
#subdir('doc/reference/protocols/facebook')
#subdir('doc/reference/pidgin')
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
#subdir('libpurple/protocols/novell')
#subdir('libpurple/protocols/null')
#subdir('libpurple/protocols/oscar')
#subdir('libpurple/protocols/oscar/tests')
#subdir('libpurple/protocols/sametime')
#subdir('libpurple/protocols/silc')
#subdir('libpurple/protocols/simple')
#subdir('libpurple/protocols/zephyr')
#subdir('libpurple/tests')
subdir('share/sounds')
subdir('share/ca-certs')
subdir('finch')
#subdir('finch/libgnt')
#subdir('finch/libgnt/wms')
#subdir('finch/plugins')
subdir('pidgin')
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
message('Protocols to build dynamically : @0@'.format(DYNAMIC_PRPLS))
message('Protocols to link statically.. : @0@'.format(STATIC_PRPLS))
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
message('Build with Secret Service..... : ' + enable_secret_service.to_string())
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
bindir = get_option('bindir')
if not bindir.startswith('/')
	bindir = get_option('prefix') + '/' + bindir
endif
message('Pidgin will be installed in @0@.'.format(bindir))
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

