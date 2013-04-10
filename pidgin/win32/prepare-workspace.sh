#!/bin/bash
# Script to prepare a workspace (devel dependencies, check system
# configuration) for Pidgin compilation under win32
#
# Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>, licensed under GNU GPL

# TODO:
# * check, if PATH was correctly set (for mingw, perl, nsis)

# configuration

BONJOUR_GUID_PACKED="5CA28B3B1DEA7654999C464610C010EB"

#TODO: this is just a temporary mirror - Tomek Wasilczyk's <tomkiewicz@cpw.pidgin.im> Dropbox
DOWNLOAD_HOST="https://dl.dropbox.com/u/5448886/pidgin-win32/devel-deps/"

ARCHIVES=""
OBS_SKIP="usr/i686-w64-mingw32/sys-root/mingw"

# bsdcpio is used for extracting rpms
ARC_CPI="https://dl.dropbox.com/u/5448886/pidgin-win32/cpio/bsdcpio-3.0.3-1.4.zip;bsdcpio;3.0.3-1.4;0cb99adb2c2d759c9a21228223e55c8bf227f736;;"
ARCHIVES+="ARC_CPI "

ARC_CSA="${DOWNLOAD_HOST}cyrus-sasl-2.1.25.tar.gz;Cyrus SASL;2.1.25;b9d7f510c0c5daa71ee5225daacdd58e948a8d19;;"
ARCHIVES+="ARC_CSA "

ARC_NSS="${DOWNLOAD_HOST}mingw32-mozilla-nss-devel-3.13.3-1.6.noarch.rpm;NSS;3.13.3-1.6;91ea7f1208264da343de51fba882e11336066190;${OBS_SKIP};nss-3.13"
ARCHIVES+="ARC_NSS "
ARC_NSP="${DOWNLOAD_HOST}mingw32-mozilla-nspr-devel-4.9-2.6.noarch.rpm;NSPR;4.9-2.6;bd348943b8250cf9fc15da0b7c743141f9404033;${OBS_SKIP};nss-3.13"
ARCHIVES+="ARC_NSP "

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
ARC_MG8="${DOWNLOAD_HOST}mingw32-zlib-1.2.7-1.4.noarch.rpm;mingw: zlib;1.2.7-1.4;83e91f3b4d14e47131ca33fc69e12b82aabdd589;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MG8 "
ARC_MG9="${DOWNLOAD_HOST}mingw32-headers-20130216-1.1.noarch.rpm;mingw: headers;20130216-1.1;313bdc131e15bbca1e4332395c536f2caa9e54b0;${OBS_SKIP}/include;mingw/lib/gcc/i686-w64-mingw32/4.8.0/include"
ARCHIVES+="ARC_MG9 "
ARC_MGA="${DOWNLOAD_HOST}mingw32-libgcc-4.8.0-6.1.noarch.rpm;mingw: libgcc;4.8.0-6.1;ab599bf07bf2d56367c57b442440598358c943af;${OBS_SKIP};mingw"
ARCHIVES+="ARC_MGA "

#gtk and friends
GTK_DIR="gtk2-2.24"
ARC_GT1="${DOWNLOAD_HOST}mingw32-glib2-devel-2.36.0-2.2.noarch.rpm;gtk: Glib;2.36.0-2.2;dd1a632960673c5a3e86a01015a15564d023f8d0;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT1 "
ARC_GT2="${DOWNLOAD_HOST}mingw32-gtk2-devel-2.24.14-2.2.noarch.rpm;gtk: GTK+2;2.24.14-2.2;8be2b1c9dc94fa9e19f9f95a9b716340bbab643f;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT2 "
ARC_GT3="${DOWNLOAD_HOST}mingw32-libintl-devel-0.18.1.1-13.6.noarch.rpm;gtk: libintl;0.18.1.1-13.6;49afd3059ecc7713debb29b801558958637114d1;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT3 "
ARC_GT4="${DOWNLOAD_HOST}mingw32-zlib-devel-1.2.7-1.7.noarch.rpm;gtk: zlib;1.2.7-1.7;e3fd07747fcd96bbf83d7a1a870feccc19c0e15e;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT4 "
ARC_GT5="${DOWNLOAD_HOST}mingw32-atk-devel-2.8.0-1.3.noarch.rpm;gtk: ATK;2.8.0-1.3;3324e25c85222a95ef12ef28305aa19680860c78;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT5 "
ARC_GT6="${DOWNLOAD_HOST}mingw32-cairo-devel-1.10.2-8.8.noarch.rpm;gtk: Cairo;1.10.2-8.8;9b6acea60968eb218a0eedbcca05820576d8d8df;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT6 "
ARC_GT7="${DOWNLOAD_HOST}mingw32-gdk-pixbuf-devel-2.26.3-1.7.noarch.rpm;gtk: GDK-PixBuf;2.26.3-1.7;6a82f4b03ed83b8d68a414d5631b85d8589471c7;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT7 "
ARC_GT8="${DOWNLOAD_HOST}mingw32-pango-devel-1.30.1-1.7.noarch.rpm;gtk: Pango;1.30.1-1.7;8cd9e088f5355f499c3b25af1a451c7e63383f98;${OBS_SKIP};${GTK_DIR}"
ARCHIVES+="ARC_GT8 "

ARC_ENC="${DOWNLOAD_HOST}mingw32-enchant-devel-1.6.0-3.7.noarch.rpm;Enchant;1.6.0-3.7;a535ac09423ba84fcd484ffa2251ab7ceea0121f;${OBS_SKIP};enchant-1.6"
ARCHIVES+="ARC_ENC "

ARC_GSP="${DOWNLOAD_HOST}mingw32-gtkspell-devel-2.0.16-2.5.noarch.rpm;GtkSpell;2.0.16-2.5;ededc099c2ad5ede6b27cf5f70753f693fa1f0e3;${OBS_SKIP};gtkspell-2.0"
ARCHIVES+="ARC_GSP "

# TODO: is it really necessary?
ARC_INT="${DOWNLOAD_HOST}intltool-0.50.2-4.1.1.noarch.rpm;intltool;0.50.2-4.1.1;92c42de2b8a9827b6dca65090dd4b0e293397689;usr;intltool-0.50"
ARCHIVES+="ARC_INT "

ARC_MWH="${DOWNLOAD_HOST}mingw32-meanwhile-devel-1.0.2-2.6.noarch.rpm;meanwhile;1.0.2-2.6;1a1ceda731486c28aca0b866e4746992bc7ba121;${OBS_SKIP};meanwhile-1.0"
ARCHIVES+="ARC_MWH "

ARC_PRL="${DOWNLOAD_HOST}perl-5.10.0.tar.gz;Perl;5.10.0;f7cb9e70260962fac8e186bfa9a108da3acd4621;perl-5.10.0;perl-5.10"
ARCHIVES+="ARC_PRL "

ARC_SIL="${DOWNLOAD_HOST}silc-toolkit-1.1.10.tar.gz;SILC Toolkit;1.1.10;42f835ed28d9567acde8bd3e553c8a5c94b799c5;silc-toolkit-1.1.10;silc-toolkit-1.1.10"
ARCHIVES+="ARC_SIL "

ARC_TCL="${DOWNLOAD_HOST}mingw32-tcl-devel-8.5.9-13.6.noarch.rpm;Tcl;8.5.9-13.6;22a6d0e748d7c7c5863f15199a21019a57a46748;${OBS_SKIP};tcl-8.5"
ARCHIVES+="ARC_TCL "

ARC_TK="${DOWNLOAD_HOST}mingw32-tk-devel-8.5.9-8.6.noarch.rpm;Tk;8.5.9-8.6;17fc995bbdca21579b52991c2e74bead1815c76a;${OBS_SKIP};tcl-8.5"
ARCHIVES+="ARC_TK "

ARC_JSG="${DOWNLOAD_HOST}mingw32-json-glib-devel-0.14.2-1.7.noarch.rpm;json-glib;0.14.2-1.7;e86a81d5c4bbc7a2ea6b808dd5819c883c4303cc;${OBS_SKIP};json-glib-0.14"
ARCHIVES+="ARC_JSG "

ARC_XML="${DOWNLOAD_HOST}mingw32-libxml2-devel-2.9.0-2.1.noarch.rpm;libxml2;2.9.0-2.1;bd63823e0be2436ee7d2369aa254e7214a0dd692;${OBS_SKIP};libxml2-2.9"
ARCHIVES+="ARC_XML "

ARC_WKG="${DOWNLOAD_HOST}mingw32-libwebkitgtk-devel-1.8.3-1.14.noarch.rpm;WebKitGTK+;1.8.3-1.14;5f2ae2c8c04c4ad4309ba677de886f450db1fe6d;${OBS_SKIP};libwebkitgtk-1.10"
ARCHIVES+="ARC_WKG "

ARC_SOU="${DOWNLOAD_HOST}mingw32-libsoup-devel-2.36.0-1.4.noarch.rpm;libsoup;2.36.0-1.4;bc0a402add235cc263b972453fe4f1afa6a07ba5;${OBS_SKIP};libsoup-2.36"
ARCHIVES+="ARC_SOU "

ARC_GTT="${DOWNLOAD_HOST}mingw32-gettext-runtime-0.18.1.1-13.6.noarch.rpm;gettext;0.18.1.1-13.6;e3785e932427d63bf5cf27f258d1236e49437143;${OBS_SKIP};gettext-0.18"
ARCHIVES+="ARC_GTT "
ARC_GTL="${DOWNLOAD_HOST}mingw32-libintl-0.18.1.1-13.4.noarch.rpm;gettext: libintl;0.18.1.1-13.4;043c3b8eb9c872681faed5ec5263456a24bf29e4;${OBS_SKIP};gettext-0.18"
ARCHIVES+="ARC_GTL "

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

function reg_get_install_path() {
	guid_packed=$1
	reg_key="/proc/registry/HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/Installer/UserData/S-1-5-18/Products/${guid_packed}/InstallProperties/InstallLocation"
	if [ -f $reg_key ] ; then
		path_win32_to_cygwin "`cat ${reg_key}`"
		reg_ret="${path_ret}"
	else
		reg_ret=""
	fi
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
	file=`$BASENAME "$url"`
	ext=`echo "$file" | $SED 's|.*\.\(.*\)|\1|'`
	
	old_dir=`pwd`
	cd "$WIN32DEV_STORE"

	echo "Installing ${name}..."
	
	rm -rf "tmp"
	mkdir "tmp"
	if [ "$ext" == "gz" ] || [ "$ext" == "bz2" ]; then
		$TAR -xf "$file" -C "tmp" || exit 1
	elif [ "$ext" == "zip" ]; then
		$UNZIP -q "$file" -d "tmp" || exit 1
	elif [ "$ext" == "rpm" ]; then
		cd "tmp"
		${WIN32DEV_BASE}/bsdcpio/bsdcpio.exe --quiet -di < "../${file}" || exit 1
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
REALPATH=`which realpath 2>/dev/null`
BASENAME=`which basename`
SED=`which sed`
CUT=`which cut`
WGET=`which wget`
SHA1SUM=`which sha1sum`
TAR=`which tar`
UNZIP=`which unzip`

if [ "$SED" == "" ] || [ "$CUT" == "" ] || [ "$BASENAME" == "" ] ||
	[ "$WGET" == "" ] || [ "$SHA1SUM" == "" ] || [ "$TAR" == "" ] ||
	[ "$UNZIP" == "" ]; then
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

if [ -e "$WIN32DEV_BASE" ]; then
	echo "win32-dev directory exists, please remove it before proceeding"
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
	echo "Bonjour SDK for Windows v3.0 is not installed, please do it."
	echo "You can download this SDK from https://developer.apple.com/bonjour/"
	echo "(Apple ID may be required)."
	exit 1;
fi

# downloading archives
echo "Downloading and verifying archives..."
for ARCHIVE in $ARCHIVES ; do
	ARCHIVE=${!ARCHIVE}
	download_archive "$ARCHIVE"
done

echo "Composing workspace..."
mkdir "$WIN32DEV_BASE" || exit 1
path_real "$WIN32DEV_BASE"
WIN32DEV_BASE="$path_ret"

echo "Installing Bonjour SDK..."
mkdir "$WIN32DEV_BASE/Bonjour_SDK"
cp -r "${BONJOUR_SDK_DIR}"/* "$WIN32DEV_BASE/Bonjour_SDK/"

for ARCHIVE in $ARCHIVES ; do
	ARCHIVE=${!ARCHIVE}
	extract_archive "$ARCHIVE"
done

echo "Removing bsdcpio..."
rm -rf "${WIN32DEV_BASE}/bsdcpio"

echo "Done."
