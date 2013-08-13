#!/bin/bash
# Script to silence win32 and perl build by displaying a neat one-line notice
# instead of full command contents when executing compilers.
#
# Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>, licensed under GNU GPL

tag=$1
found=0
object=""
file_1=""
file_2=""
is_final=0
for arg in "$@"
do
	if [ "$found" == 1 ]; then
		object="$arg"
		break
	fi
	if [ "$arg" == "-o" ]; then
		found=1
	fi
	if [ "$tag" == "auto" ] && [ "$arg" == "-shared" ]; then
		tag="CCLD"
	fi
	if [ "$tag" == "PERL" ] && [ "${arg%(*}" == "Mkbootstrap" ]; then
		object="${arg%;}"
		is_final=1
		break
	fi
	ext_1=${arg#${arg%??}}
	if [ "${ext_1}" == ".c" ]; then
		file_1="$arg"
	fi
	ext_2=${arg#${arg%???}}
	if [ "${ext_2}" == ".xs" ]; then
		file_2="$arg"
	fi
	ext_3=${arg#${arg%????}}
	if [ "${ext_3}" == ".3pm" ]; then
		file_2="$arg"
	fi
done

if [ "$tag" == "auto" ]; then
	tag="CC"
fi

if [ "$tag" == "PERL" ] && [ "$is_final" == 0 ]; then
	object=`echo "$object" | sed -n 's|.*output *=> *"\([^"]*\)".*|\1|p'`
fi

if [ "$object" == "" ] && [ "${file_1}" != "" ]; then
	object="${file_1}"
fi
if [ "$object" == "" ] && [ "${file_2}" != "" ]; then
	object="${file_2}"
fi

shift 1
if [ "$object" == "" ]; then
	echo "$@" >&2
else
	echo -e "  $tag\t$object" >&2
fi
"$@"
