#!/bin/bash
# Script to prepare a workspace (devel dependencies, check system
# configuration) for Pidgin compilation under win32
#
# Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>, licensed under GNU GPL

# configuration

BONJOUR_GUID_PACKED="5CA28B3B1DEA7654999C464610C010EB 2EA34582882FE334694F0BCD7D8DE336"
ACTIVEPERL_GUID_PACKED="BC98F31FB8440B94CB3674649419766C 547A2C684F806164DB756F228DAB5840 5E7EC16051106BB43818746C209BC8D7"
PERL_DIR_FALLBACK="/cygdrive/c/Perl/bin"
NSIS_DIR_REGKEY="HKEY_LOCAL_MACHINE/SOFTWARE/NSIS/@"

DEBUG_SKIP_DOWNLOADING=0
DEBUG_SKIP_INSTALL=0

#TODO: this is just a temporary mirror - Tomek Wasilczyk's <tomkiewicz@cpw.pidgin.im> Dropbox
DOWNLOAD_HOST="https://dl.dropbox.com/u/5448886/pidgin-win32/devel-deps/"

ARCHIVES=""
OBS_SKIP="usr/i686-w64-mingw32/sys-root/mingw"

# bsdcpio is used for extracting rpms
ARC_CPI="https://dl.dropbox.com/u/5448886/pidgin-win32/cpio/bsdcpio-3.0.3-1.4.tar.gz;bsdcpio;3.0.3-1.4;0460c7a52f8c93d3c4822d6d1aaf9410f21bd4da;bsdcpio-3.0.3-1.4;bsdcpio"
ARCHIVES+="ARC_CPI "

ARC_CSA="${DOWNLOAD_HOST}cyrus-sasl-2.1.25.tar.gz;Cyrus SASL;2.1.25;b9d7f510c0c5daa71ee5225daacdd58e948a8d19;cyrus-sasl-2.1.25;cyrus-sasl-2.1"
ARCHIVES+="ARC_CSA "

ARC_NSS="${DOWNLOAD_HOST}mingw32-mozilla-nss-devel-3.14.3-2.2.noarch.rpm;NSS;3.14.3-2.2;fd394678ef2a8ef1dbbc20c25701678bc8678acf;${OBS_SKIP};nss-3.14"
ARCHIVES+="ARC_NSS "
ARC_NSP="${DOWNLOAD_HOST}mingw32-mozilla-nspr-devel-4.9.6-4.1.noarch.rpm;NSPR;4.9.6-4.1;b15aefbf99ade3042d0e4ed32f9368ff38064ecd;${OBS_SKIP};nss-3.14"
ARCHIVES+="ARC_NSP "

ARC_GTLS="${DOWNLOAD_HOST}mingw32-libgnutls-devel-2.12.22-2.2.noarch.rpm;GnuTLS;2.12.22-2.2;22ae0425842b2c905bdbb93e8e5f3f813db4680f;${OBS_SKIP};gnutls-2.12"
ARCHIVES+="ARC_GTLS "

ARC_PID="${DOWNLOAD_HOST}pidgin-inst-deps-20130214.tar.gz;inst-deps;20130214;372218ab472c4070cd45489dae175dea5638cf17;;"
ARCHIVES+="ARC_PID "

#mingw gcc and its dependencies
ARC_MG1="${DOWNLOAD_HOST}mingw32-gcc-4.8.0-6.1.noarch.rpm;mingw: gcc;4.8.0-6.1;00591ba625cb4d3968f9907a76e7e3350e80c65b;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG1 "
ARC_MG2="${DOWNLOAD_HOST}mingw32-cpp-4.8.0-6.1.noarch.rpm;mingw: cpp;4.8.0-6.1;ea22584abf14cdf34217bb5eb24a30211a382882;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG2 "
ARC_MG3="${DOWNLOAD_HOST}mingw32-binutils-2.22.52-3.5.noarch.rpm;mingw: binutils;2.22.52-3.5;e6431d8dfa0dfe5a3488017c291cb68193999808;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG3 "
ARC_MG4="${DOWNLOAD_HOST}mingw32-libgmp-5.0.5-1.6.noarch.rpm;mingw: libgmp;5.0.5-1.6;58ff8155e870063a2cab999f413ffa1ec6ad2d16;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG4 "
ARC_MG5="${DOWNLOAD_HOST}mingw32-libmpc-1.0-1.6.noarch.rpm;mingw: libmpc;1.0-1.6;552dd1de81aef3dfdb7b3a87f13b79e6805d9940;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG5 "
ARC_MG6="${DOWNLOAD_HOST}mingw32-libmpfr-3.1.0-1.6.noarch.rpm;mingw: libmpfr;3.1.0-1.6;d86d12af65c442dc260d156528fff009d21dab9c;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG6 "
ARC_MG7="${DOWNLOAD_HOST}mingw32-runtime-20130216-2.3.noarch.rpm;mingw: runtime;20130216-2.3;9ff3810f8313d19ab18458d73565856608cf9188;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG7 "
ARC_MG8="${DOWNLOAD_HOST}mingw32-zlib-1.2.7-1.7.noarch.rpm;mingw: zlib;1.2.7-1.7;c34986df8520de706f9e8516f4353af90ba78f39;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG8 "
ARC_MG9="${DOWNLOAD_HOST}mingw32-headers-20130216-1.1.noarch.rpm;mingw: headers;20130216-1.1;313bdc131e15bbca1e4332395c536f2caa9e54b0;${OBS_SKIP}/include;mingw/lib/gcc/i686-w64-mingw32/4.8.0/include"
ARCHIVES+="ARC_MG9 "
ARC_MGA="${DOWNLOAD_HOST}mingw32-libgcc-4.8.0-6.1.noarch.rpm;mingw: libgcc;4.8.0-6.1;ab599bf07bf2d56367c57b442440598358c943af;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MGA "

#gtk and friends
GTK_DIR="gtk2-2.24"
ARC_GT1="${DOWNLOAD_HOST}mingw32-glib2-devel-2.36.1-1.1.noarch.rpm;gtk: Glib;2.36.1-1.1;af64b014c735cbdb750e35960c0fde9de4fef9f0;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT1 "
ARC_GT2="${DOWNLOAD_HOST}mingw32-gtk2-devel-2.24.14-2.7.noarch.rpm;gtk: GTK+2;2.24.14-2.7;4abd5fddf7ca2b6ee7ab35f4b549894bc146a005;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT2 "
ARC_GT3="${DOWNLOAD_HOST}mingw32-libintl-devel-0.18.1.1-13.6.noarch.rpm;gtk: libintl;0.18.1.1-13.6;49afd3059ecc7713debb29b801558958637114d1;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT3 "
ARC_GT4="${DOWNLOAD_HOST}mingw32-zlib-devel-1.2.7-1.7.noarch.rpm;gtk: zlib;1.2.7-1.7;e3fd07747fcd96bbf83d7a1a870feccc19c0e15e;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT4 "
ARC_GT5="${DOWNLOAD_HOST}mingw32-atk-devel-2.8.0-1.5.noarch.rpm;gtk: ATK;2.8.0-1.5;d6c54241ef3ce80b4a6722f23fe47eba88e0a9f0;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT5 "
ARC_GT6="${DOWNLOAD_HOST}mingw32-cairo-devel-1.10.2-8.12.noarch.rpm;gtk: Cairo;1.10.2-8.12;a9ea09988bc896226971dc544d9b499882d37ba6;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT6 "
ARC_GT7="${DOWNLOAD_HOST}mingw32-gdk-pixbuf-devel-2.28.0-1.2.noarch.rpm;gtk: GDK-PixBuf;2.28.0-1.2;d476228dd6e1ad43bbf0dd5d6e9e9bad394c9ec5;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT7 "
ARC_GT8="${DOWNLOAD_HOST}mingw32-pango-devel-1.34.0-2.3.noarch.rpm;gtk: Pango;1.34.0-2.3;c875ae60dacf05b642d7da5f289a3c58ff9b0e52;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT8 "
ARC_GT9="${DOWNLOAD_HOST}mingw32-glib2-2.36.1-1.1.noarch.rpm;gtk: Glib runtimes;2.36.1-1.1;ed468f064f61c5a12b716c83cba8ccbe05d22992;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT9 "
ARC_G10="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-13.6.noarch.rpm;gtk: libintl;0.18.1.1-13.6;0e6fde8e86788874366f308e25634f95613e906a;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_G10 "

ARC_ENC="${DOWNLOAD_HOST}mingw32-enchant-devel-1.6.0-3.9.noarch.rpm;Enchant;1.6.0-3.9;1daadbb4fbeb06a6ad26bed916dc2a980d971c49;${OBS_SKIP};enchant-1.6"
ARCHIVES+="ARC_ENC "

ARC_GSP="${DOWNLOAD_HOST}mingw32-gtkspell-devel-2.0.16-2.10.noarch.rpm;GtkSpell;2.0.16-2.10;efbd58f41d9053c17eb2c6ea75dff9017068e01c;${OBS_SKIP};gtkspell-2.0"
ARCHIVES+="ARC_GSP "

# TODO: is it really necessary?
ARC_INT="${DOWNLOAD_HOST}intltool-0.50.2-4.1.1.noarch.rpm;intltool;0.50.2-4.1.1;92c42de2b8a9827b6dca65090dd4b0e293397689;usr;intltool-0.50"
ARCHIVES+="ARC_INT "

ARC_MWH="${DOWNLOAD_HOST}mingw32-meanwhile-devel-1.0.2-3.2.noarch.rpm;meanwhile;1.0.2-3.2;2c92bbf6084cb930c923ec94c17b62b4b894c146;${OBS_SKIP};meanwhile-1.0"
ARCHIVES+="ARC_MWH "
ARC_MWHD="${DOWNLOAD_HOST}mingw32-meanwhile-debug-1.0.2-3.2.noarch.rpm;meanwhile debug symbols;1.0.2-3.2;7e3c02178d219426eeb8f4f34147763c7ea5be85;${OBS_SKIP};meanwhile-1.0"
ARCHIVES+="ARC_MWHD "

ARC_PRL="${DOWNLOAD_HOST}perl-5.10.0.tar.gz;Perl;5.10.0;46496029a80cabdfa119cbd70bc14d14bfde8071;perl-5.10.0;perl-5.10"
ARCHIVES+="ARC_PRL "

ARC_SIL="${DOWNLOAD_HOST}mingw32-silc-toolkit-devel-1.1.10-2.1.noarch.rpm;SILC Toolkit;1.1.10-2.1;cc92fc87c013a085bdd0664e8fba1acc5a2ccb18;${OBS_SKIP};silc-toolkit-1.1"
ARCHIVES+="ARC_SIL "

ARC_TCL="${DOWNLOAD_HOST}mingw32-tcl-devel-8.5.9-14.1.noarch.rpm;Tcl;8.5.9-14.1;22a64967654629e01a2f52226c3de431a43683f8;${OBS_SKIP};tcl-8.5;include/tcl-private/generic/(tcl|tclDecls|tclPlatDecls|tclTomMath|tclTomMathDecls)\\.h"
ARCHIVES+="ARC_TCL "

ARC_TK="${DOWNLOAD_HOST}mingw32-tk-devel-8.5.9-8.7.noarch.rpm;Tk;8.5.9-8.7;c469e5933cace0f2eed0fec9892843ca216c51ea;${OBS_SKIP};tcl-8.5;include/tk-private/generic/(tk|tkDecls|tkIntXlibDecls|tkPlatDecls)\\.h"
ARCHIVES+="ARC_TK "

ARC_JSG="${DOWNLOAD_HOST}mingw32-json-glib-devel-0.14.2-2.1.noarch.rpm;json-glib;0.14.2-2.1;27154ec4e4fa214b72f28658be2de7be4e0a9e3e;${OBS_SKIP};json-glib-0.14"
ARCHIVES+="ARC_JSG "

ARC_XML="${DOWNLOAD_HOST}mingw32-libxml2-devel-2.9.0-2.1.noarch.rpm;libxml2;2.9.0-2.1;bd63823e0be2436ee7d2369aa254e7214a0dd692;${OBS_SKIP};libxml2-2.9"
ARCHIVES+="ARC_XML "

ARC_WKG="${DOWNLOAD_HOST}mingw32-libwebkitgtk-devel-1.10.2-9.2.noarch.rpm;WebKitGTK+;1.10.2-9.2;02cd5de75e3b4269bc1a31320e95f455d5804be9;${OBS_SKIP};libwebkitgtk-1.10"
ARCHIVES+="ARC_WKG "

ARC_SOU="${DOWNLOAD_HOST}mingw32-libsoup-devel-2.42.2-1.1.noarch.rpm;libsoup;2.42.2-1.1;cb4e520f1bb17c83230f28bb225420dce54c8d80;${OBS_SKIP};libsoup-2.42"
ARCHIVES+="ARC_SOU "

ARC_GTT="${DOWNLOAD_HOST}mingw32-gettext-runtime-0.18.1.1-13.6.noarch.rpm;gettext;0.18.1.1-13.6;e3785e932427d63bf5cf27f258d1236e49437143;${OBS_SKIP};gettext-0.18"
ARCHIVES+="ARC_GTT "
ARC_GTL="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-13.6.noarch.rpm;gettext: libintl;0.18.1.1-13.6;0e6fde8e86788874366f308e25634f95613e906a;${OBS_SKIP};gettext-0.18"
ARCHIVES+="ARC_GTL "

ARC_VV1="${DOWNLOAD_HOST}mingw32-gstreamer-devel-0.10.36-10.1.noarch.rpm;gstreamer;0.10.36-10.1;a54b53b31a47dd3d4243b8e772553e0b05430aaf;${OBS_SKIP};gstreamer-0.10"
ARCHIVES+="ARC_VV1 "
ARC_VV2="${DOWNLOAD_HOST}mingw32-gst-plugins-base-devel-0.10.36-15.1.noarch.rpm;gst-plugins-base;0.10.36-15.1;5bc0d94abdce4f2f2bafceda8046f01a5b29bd71;${OBS_SKIP};gstreamer-0.10"
ARCHIVES+="ARC_VV2 "
ARC_VV3="${DOWNLOAD_HOST}mingw32-farstream-devel-0.1.2-19.1.noarch.rpm;farstream;0.1.2-19.1;6c9f29de289b661d192c88998ed5bdf17de7bcec;${OBS_SKIP};gstreamer-0.10"
ARCHIVES+="ARC_VV3 "

# implementation

if [ `uname -o` != "Cygwin" ]; then
	echo
	echo "WARNING: You are on a non-Cygwin platform! Your mileage may vary."
	echo
fi

function path_win32_to_cygwin() {
	path_ret=`echo "$1" | $SED 's|\\\\|/|g; s|\\(.\\):|/cygdrive/\\1|; s| |\\ |g'`
}

function path_real() {
	if [ "$REALPATH" != "" ]; then
		path_ret="`${REALPATH} "$1"`"
	else
		path_ret="$1"
	fi
}

function reg_get_path() {
	reg_ret=""
	reg_key="/proc/registry/$1"
	if [ ! -f $reg_key ] ; then
		reg_key="/proc/registry64/$1"
	fi
	if [ -f $reg_key ] ; then
		path_win32_to_cygwin "`cat ${reg_key}`"
		reg_ret="${path_ret}"
		return 0
	fi
	return 1
}

function reg_get_install_path() {
	reg_ret=""
	for guid_packed in $1 ; do
		reg_get_path "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/Installer/UserData/S-1-5-18/Products/${guid_packed}/InstallProperties/InstallLocation" && return 0
	done
	return 1
}

function check_path() {
	chk_cmd="$1"
	expected="$2"
	
	expected=`${REALPATH} -e "$expected"`
	current=`which "${chk_cmd}" 2> /dev/null`
	if [ "$expected" == "" ]; then
		echo "Error while checking path"
		exit 1
	fi
	
	if [ "$expected" != "$current" ]; then
		dir=`dirname "${expected}"` || exit 1
		echo "Adding $dir to PATH"
		echo "" >> ~/.bashrc
		echo "export PATH=\"$dir\":\$PATH" >> ~/.bashrc
		return 1
	fi
	return 0
}

function download() {
	if [ -e "$2" ]; then
		echo "File exists"
		exit 1
	fi
	failed=0
	$WGET -t 3 "$1" -O "$2" -o "wget.log" --retry-connrefused --waitretry=2 --ca-certificate="$WIN32DEV_STORE/cacert.pem" || failed=1
	if [ $failed != 0 ]; then
		echo "Download failed"
		cat "wget.log"
		rm "wget.log"
		rm -f "$2"
		exit 1
	fi
	rm "wget.log"
}

function sha1sum_calc() {
	sha1sum_ret=`$SHA1SUM "$1" | $SED 's| .*||'`
}

function download_archive() {
	url=`echo "$1" | $CUT -d';' -f1`
	name=`echo "$1" | $CUT -d';' -f2`
	version=`echo "$1" | $CUT -d';' -f3`
	sha1sum_orig=`echo "$1" | $CUT -d';' -f4`
	file=`$BASENAME "$url"`
	
	cd "$WIN32DEV_STORE"
	
	if [ ! -e "$file" ]; then
		echo "Downloading ${name} ${version}..."
		download "$url" "$file"
	fi
	
	sha1sum_calc "$file"
	sha1sum_file="$sha1sum_ret"
	
	if [ "$sha1sum_file" != "$sha1sum_orig" ]; then
		echo "sha1sum ($sha1sum_file) for $file doesn't match expected value of $sha1sum_orig"
		exit 1
	fi
	
	cd - > /dev/null
}

function extract_archive() {
	url=`echo "$1" | $CUT -d';' -f1`
	name=`echo "$1" | $CUT -d';' -f2`
	dir_skip=`echo "$1" | $CUT -d';' -f5`
	dir_extract=`echo "$1" | $CUT -d';' -f6`
	filter_output=`echo "$1" | $CUT -d';' -f7`
	file=`$BASENAME "$url"`
	ext=`echo "$file" | $SED 's|.*\.\(.*\)|\1|'`
	
	old_dir=`pwd`
	cd "$WIN32DEV_STORE"

	if [ "${filter_output}" == "" ]; then
		# don't match anything (only rpm)
		filter_output="$$"
	fi
	
	if [ "$ext" == "patch" ]; then
		echo "Applying ${name}..."
		
		old_tmp="$TMP"
		TMP="."
		
		patch --strip=${dir_skip} --directory="${WIN32DEV_BASE}/${dir_extract}" --forward --quiet --input="$WIN32DEV_STORE/${file}" || exit 1
		
		TMP="${old_tmp}"
		
		cd "${old_dir}"
		continue
	fi
	
	echo "Installing ${name}..."
	
	rm -rf "tmp"
	mkdir "tmp"
	if [ "$ext" == "gz" ] || [ "$ext" == "bz2" ]; then
		$TAR -xf "$file" -C "tmp" || exit 1
	elif [ "$ext" == "zip" ]; then
		$UNZIP -q "$file" -d "tmp" || exit 1
	elif [ "$ext" == "rpm" ]; then
		cd "tmp"
		( ${WIN32DEV_BASE}/bsdcpio/bsdcpio.exe --quiet -di < "../${file}" 2>&1 ) | grep -v -P "${filter_output}" 1>&2
		cd ..
	else
		echo "Uknown extension: $ext"
		rm -rf "tmp"
		exit 1
	fi

	dst_dir="${WIN32DEV_BASE}/${dir_extract}"
	src_dir="tmp/${dir_skip}"
	mkdir -p $dst_dir
	cp -rf "${src_dir}"/* "${dst_dir}"/ || exit 1
	rm -rf "tmp"
	
	cd "${old_dir}"
}

# required and optional system dependencies
REALPATH=`which realpath`
BASENAME=`which basename`
SED=`which sed`
CUT=`which cut`
WGET=`which wget`
SHA1SUM=`which sha1sum`
TAR=`which tar`
UNZIP=`which unzip`

if [ "$SED" == "" ] || [ "$CUT" == "" ] || [ "$BASENAME" == "" ] ||
	[ "$WGET" == "" ] || [ "$SHA1SUM" == "" ] || [ "$TAR" == "" ] ||
	[ "$UNZIP" == "" ] || [ "$REALPATH" == "" ]; then
	echo
	echo ERROR: One or more required utilities were not found. Use Cygwin\'s setup.exe to
	echo install all packages listed above.
	exit 1
fi

# determining paths

PIDGIN_BASE="`pwd`/../.."
WIN32DEV_BASE="${PIDGIN_BASE}/../win32-dev"
WIN32DEV_STORE="${PIDGIN_BASE}/../win32-dev-store"

if [ ! -e "${PIDGIN_BASE}/ChangeLog" ]; then
	echo "Pidgin base directory not found"
	exit 1
fi

if [ -e "$WIN32DEV_BASE" ] && [ $DEBUG_SKIP_INSTALL == 0 ]; then
	echo "win32-dev directory ($(readlink -f $WIN32DEV_BASE)) exists, please remove it before proceeding"
	exit 1
fi

mkdir -p "$WIN32DEV_STORE" || exit 1
path_real "$PIDGIN_BASE"
PIDGIN_BASE="$path_ret"
path_real "$WIN32DEV_STORE"
WIN32DEV_STORE="$path_ret"

cat "$PIDGIN_BASE/share/ca-certs"/*.pem > "$WIN32DEV_STORE/cacert.pem"

# checking for Bonjour SDK

reg_get_install_path "$BONJOUR_GUID_PACKED"
BONJOUR_SDK_DIR=$reg_ret

if [ "$BONJOUR_SDK_DIR" == "" ]; then
	echo "Bonjour SDK for Windows v3.0/v2.0.4 is not installed, please do it."
	echo "You can download this SDK from https://developer.apple.com/bonjour/"
	echo "(Apple ID may be required)."
	exit 1;
fi

# checking for Perl

reg_get_install_path "$ACTIVEPERL_GUID_PACKED"
ACTIVEPERL_DIR=$reg_ret
PERL_DIR=""
if [ "$ACTIVEPERL_DIR" != "" ]; then
	PERL_DIR="$ACTIVEPERL_DIR/bin"
else
	PERL_DIR="$PERL_DIR_FALLBACK"
fi

if ! ${REALPATH} -e "${PERL_DIR}/perl" &> /dev/null ; then
	echo "Perl not found in \"${PERL_DIR}\", please install it."
	exit 1
fi

# checking for NSIS

reg_get_path "${NSIS_DIR_REGKEY}"
NSIS_DIR=$reg_ret
if [ "${NSIS_DIR}" == "" ]; then
	echo "NSIS not found, please install it."
	exit 1
fi

if ! ${REALPATH} -e "${NSIS_DIR}/Plugins/nsisunz.dll" &> /dev/null ; then
	echo "NSIS plugin \"nsisunz.dll\" not found in \"${NSIS_DIR}/Plugins\", please install it."
	exit 1
fi

if ! ${REALPATH} -e "${NSIS_DIR}/Plugins/SHA1Plugin.dll" &> /dev/null ; then
	echo "NSIS plugin \"SHA1Plugin.dll\" not found in \"${NSIS_DIR}/Plugins\", please install it."
	exit 1
fi

# downloading archives
if [ $DEBUG_SKIP_DOWNLOADING == 0 ]; then
echo "Downloading and verifying archives..."
for ARCHIVE in $ARCHIVES ; do
	ARCHIVE=${!ARCHIVE}
	download_archive "$ARCHIVE"
done
fi

echo "Composing workspace..."
if [ $DEBUG_SKIP_INSTALL == 0 ]; then
mkdir "$WIN32DEV_BASE" || exit 1
fi
path_real "$WIN32DEV_BASE"
WIN32DEV_BASE="$path_ret"

if [ $DEBUG_SKIP_INSTALL == 0 ]; then
echo "Installing Bonjour SDK..."
mkdir "$WIN32DEV_BASE/bonjour-sdk"
cp -r "${BONJOUR_SDK_DIR}"/* "$WIN32DEV_BASE/bonjour-sdk/"

for ARCHIVE in $ARCHIVES ; do
	ARCHIVE=${!ARCHIVE}
	extract_archive "$ARCHIVE"
done
fi

echo "Removing bsdcpio..."
rm -rf "${WIN32DEV_BASE}/bsdcpio"

echo "Checking PATH..."
path_changed=0
check_path "gcc" "${WIN32DEV_BASE}/mingw/bin/gcc" || path_changed=1
check_path "perl" "${PERL_DIR}/perl" || path_changed=1
check_path "makensis" "${NSIS_DIR}/makensis" || path_changed=1
if [ $path_changed == 1 ]; then
	echo "PATH changed - executing sub-shell"
	bash
	echo "This session uses outdated PATH variable - please exit"
	exit 1
fi

echo "Done."
