#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

#
# Generate tags or gtags or cscope or etags files
#

verbose=false
linux=true
bsd=true
x86_32=true
x86_64=true
ppc_64=true
arm_32=true
arm_64=true

print_usage()
{
	echo "Usage: $(basename $0) [-h] [-v] tags|cscope|gtags|etags [config]"
	echo "Examples of valid configs are: "
	echo "x86_64-bsd, arm64-linux, ppc_64-linux"
}

# Move to the root of the git tree
cd $(dirname $0)/..

while getopts hv ARG ; do
	case $ARG in
		v ) verbose=true ;;
		h ) print_usage; exit 0 ;;
		? ) print_usage; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

#ignore version control files
ignore="( -name .svn -o -name CVS -o -name .hg -o -name .git ) -prune -o"

source_dirs="app buildtools drivers examples lib"

skip_bsd="( -name freebsd ) -prune -o"
skip_linux="( -name linux ) -prune -o"
skip_arch="( -name arch ) -prune -o"
skip_sse="( -name *_sse*.[chS] ) -prune -o"
skip_avx="( -name *_avx*.[chS] ) -prune -o"
skip_neon="( -name *_neon*.[chS] ) -prune -o"
skip_altivec="( -name *_altivec*.[chS] ) -prune -o"
skip_arm64="( -name *arm64*.[chS] ) -prune -o"
skip_x86="( -name *x86*.[chS] ) -prune -o"
skip_32b_files="( -name *_32.h ) -prune -o"
skip_64b_files="( -name *_64.h ) -prune -o"

skiplist="$skip_bsd $skip_linux $skip_arch $skip_sse $skip_avx \
		 $skip_neon $skip_altivec $skip_x86 $skip_arm64"

find_sources()
{
	find $1 $ignore $3 -name $2 -not -type l -print
}

common_sources()
{
	find_sources "$source_dirs" '*.[chS]' "$skiplist"
}

linux_sources()
{
	find_sources "lib/eal/linux" '*.[chS]'
}

bsd_sources()
{
	find_sources "lib/eal/freebsd" '*.[chS]'
	find_sources "kernel/freebsd" '*.[chS]'
}

arm_common()
{
	find_sources "$source_dirs" '*neon*.[chS]'
}

arm_32_sources()
{
	arm_common
	find_sources "lib/eal/arm" '*.[chS]' \
					"$skip_64b_files"
}

arm_64_sources()
{
	arm_common
	find_sources "lib/eal/arm" '*.[chS]' \
					 "$skip_32b_files"
	find_sources "$source_dirs" '*arm64.[chS]'
}

x86_common()
{
	find_sources "$source_dirs" '*_sse*.[chS]'
	find_sources "$source_dirs" '*_avx*.[chS]'
	find_sources "$source_dirs" '*x86.[chS]'
}

x86_32_sources()
{
	x86_common
	find_sources "lib/eal/x86" '*.[chS]' \
					"$skip_64b_files"
}

x86_64_sources()
{
	x86_common
	find_sources "lib/eal/x86" '*.[chS]' \
					"$skip_32b_files"
}

ppc_64_sources()
{
	find_sources "lib/eal/ppc" '*.[chS]'
	find_sources "$source_dirs" '*altivec*.[chS]'
}

if [ -n "$2" ]; then
	echo $2 | grep -q "linux" || linux=false
	echo $2 | grep -q "bsd" || bsd=false
	echo $2 | grep -q "x86_64-" || x86_64=false
	echo $2 | grep -q "arm-" || arm_32=false
	echo $2 | grep -q "arm64-" || arm_64=false
	echo $2 | grep -q "ppc_64-" || ppc_64=false
	echo $2 | grep -q -e "i686-" -e "x32-" || x86_32=false
fi

all_sources()
{
	common_sources
	if $linux ; then linux_sources ; fi
	if $bsd ; then bsd_sources ; fi
	if $x86_64 ; then x86_64_sources ; fi
	if $x86_32 ; then x86_32_sources ; fi
	if $ppc_64 ; then ppc_64_sources ; fi
	if $arm_32 ; then arm_32_sources ; fi
	if $arm_64 ; then arm_64_sources ; fi
}

show_flags()
{
	if $verbose ; then
		echo "mode:     $1"
		echo "config:   $2"
		echo "linux:    $linux"
		echo "bsd:      $bsd"
		echo "x86_32:   $x86_32"
		echo "x86_64:   $x86_64"
		echo "ppc_64:   $ppc_64"
		echo "arm_32:   $arm_32"
		echo "arm_64:   $arm_64"
	fi
}

case "$1" in
	"cscope")
		show_flags $1 $2
		all_sources > cscope.files
		cscope -q -b -f cscope.out
		;;
	"gtags")
		show_flags $1 $2
		all_sources | gtags -i -f -
		;;
	"tags")
		show_flags $1 $2
		rm -f tags
		all_sources | xargs ctags -a
		;;
	"etags")
		show_flags $1 $2
		rm -f TAGS
		all_sources | xargs etags -a
		;;
	*)
		echo "Invalid mode: $1"
		print_usage
		;;
esac
