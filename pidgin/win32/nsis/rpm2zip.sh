#!/bin/sh

here=`pwd`
for F in $*; do
    case $F in
        mingw32-*.noarch.rpm|mingw64-*.noarch.rpm|*/mingw32-*.noarch.rpm|*/mingw64-*.noarch.rpm)
        package=`rpm -qp $F 2>/dev/null`
        case $package in
            mingw32-*|mingw64-*)
            case $package in
                mingw32-*)
                cpu=i686
                bits=32
                ;;
                mingw64-*)
                cpu=x86_64
                bits=64
                ;;
            esac
            origname=`rpm -qp --queryformat='%{NAME}'  $F 2>/dev/null`
            name=$origname
            case $name in
                *-devel)
                name=${name%el}
                ;;
            esac
            shortpackage="$name"_`rpm -qp --queryformat='%{VERSION}-%{RELEASE}'_win${bits} $F 2>/dev/null`
            shortpackage=${shortpackage#mingw32-}
            shortpackage=${shortpackage#mingw64-}
            shortname=$name
            shortname=${shortname#mingw32-}
            shortname=${shortname#mingw64-}
            tmp=`mktemp -d`
            #rpm2cpio $F | lzcat | (cd $tmp && cpio --quiet -id)
            rpm2cpio $F |  (cd $tmp && cpio --quiet -id)
            (
                cd $tmp
                zipfile="$here/$shortpackage.zip"
                rm -f $zipfile
                (cd usr/${cpu}-pc-mingw32/sys-root/mingw && zip -q -r -D $zipfile .)
                if [ -d usr/share/doc/packages/$origname ] ; then
                    mv usr/share/doc/packages/$origname usr/share/doc/packages/$shortname
                    (cd usr && zip -q -r -D $zipfile share/doc/packages/$shortname)
                fi
                mkdir -p manifest
                unzip -l $zipfile >manifest/$shortpackage.mft
                zip -q $zipfile manifest/$shortpackage.mft
                N=`unzip -l $zipfile | wc -l | sed -e 's/^ *\([0-9]*\).*/\1/'`
                Nm1=`expr $N - 1`
                unzip -l $zipfile | sed -e "1,3 d" -e "$Nm1,$N d" | awk '{print $4}' | grep -v -E '/$' >manifest/$shortpackage.mft
                zip -q $zipfile manifest/$shortpackage.mft
                echo $zipfile
            )
            rm -rf $tmp
            ;;
            *)
            echo $F is not a mingw32/64 RPM package >&2
            ;;
        esac
        ;;
        *)
        echo $F is not a mingw32/64 RPM package >&2
        ;;
    esac
done
