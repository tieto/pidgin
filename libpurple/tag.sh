#!/bin/bash
# Script to silence win32 and perl build by displaying a neat one-line notice
# instead of full command contents when executing compilers.
#
# Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>, licensed under GNU GPL

tag=$1
found=0
object=""
c_file=""
xs_file=""
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
	if [ "$tag" == "PERL" ] && [ "$arg" == "-e" ]; then
		found=1
	fi
	ext_1=${arg#${arg%??}}
	if [ "${ext_1}" == ".c" ]; then
		c_file="$arg"
	fi
	ext_2=${arg#${arg%???}}
	if [ "${ext_2}" == ".xs" ]; then
		xs_file="$arg"
	fi
done

if [ "$tag" == "auto" ]; then
	tag="CC"
fi

if [ "$tag" == "PERL" ]; then
	object=`echo "$object" | sed -n 's|.*output *=> *"\([^"]*\)".*|\1|p'`
fi

if [ "$object" == "" ] && [ "${c_file}" != "" ]; then
	object="${c_file}"
fi
if [ "$object" == "" ] && [ "${xs_file}" != "" ]; then
	object="${xs_file}"
fi

shift 1
if [ "$object" == "" ]; then
	echo "$@" > /dev/stderr
else
	echo -e "  $tag\t$object" > /dev/stderr
fi
"$@"
