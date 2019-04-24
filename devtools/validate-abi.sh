#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Neil Horman. All rights reserved.
# Copyright(c) 2017 6WIND S.A.
# All rights reserved

set -e

abicheck=abi-compliance-checker
abidump=abi-dumper
default_dst=abi-check
default_target=x86_64-native-linuxapp-gcc

# trap on error
err_report() {
    echo "$0: error at line $1"
}
trap 'err_report $LINENO' ERR

print_usage () {
	cat <<- END_OF_HELP
	$(basename $0) [options] <rev1> <rev2>

	This script compares the ABI of 2 git revisions of the current
	workspace. The output is a html report and a compilation log.

	The objective is to make sure that applications built against
	DSOs from the first revision can still run when executed using
	the DSOs built from the second revision.

	<rev1> and <rev2> are git commit id or tags.

	Options:
	  -h		show this help
	  -j <num>	enable parallel compilation with <num> threads
	  -v		show compilation logs on the console
	  -d <dir>	change working directory (default is ${default_dst})
	  -t <target>	the dpdk target to use (default is ${default_target})
	  -f		overwrite existing files in destination directory

	The script returns 0 on success, or the value of last failing
	call of ${abicheck} (incompatible abi or the tool has run with errors).
	The errors returned by ${abidump} are ignored.

	END_OF_HELP
}

# log in the file, and on stdout if verbose
# $1: level string
# $2: string to be logged
log() {
	echo "$1: $2"
	if [ "${verbose}" != "true" ]; then
		echo "$1: $2" >&3
	fi
}

# launch a command and log it, taking care of surrounding spaces with quotes
cmd() {
	local i s whitespace ret
	s=""
	whitespace="[[:space:]]"
	for i in "$@"; do
		if [[ $i =~ $whitespace ]]; then
			i=\"$i\"
		fi
		if [ -z "$s" ]; then
			s="$i"
		else
			s="$s $i"
		fi
	done

	ret=0
	log "CMD" "$s"
	"$@" || ret=$?
	if [ "$ret" != "0" ]; then
		log "CMD" "previous command returned $ret"
	fi

	return $ret
}

# redirect or copy stderr/stdout to a file
# the syntax is unfamiliar, but it makes the rest of the
# code easier to read, avoiding the use of pipes
set_log_file() {
	# save original stdout and stderr in fd 3 and 4
	exec 3>&1
	exec 4>&2
	# create a new fd 5 that send to a file
	exec 5> >(cat > $1)
	# send stdout and stderr to fd 5
	if [ "${verbose}" = "true" ]; then
		exec 1> >(tee /dev/fd/5 >&3)
		exec 2> >(tee /dev/fd/5 >&4)
	else
		exec 1>&5
		exec 2>&5
	fi
}

# Make sure we configure SHARED libraries
# Also turn off IGB and KNI as those require kernel headers to build
fixup_config() {
	local conf=config/defconfig_$target
	cmd sed -i -e"$ a\CONFIG_RTE_BUILD_SHARED_LIB=y" $conf
	cmd sed -i -e"$ a\CONFIG_RTE_NEXT_ABI=n" $conf
	cmd sed -i -e"$ a\CONFIG_RTE_EAL_IGB_UIO=n" $conf
	cmd sed -i -e"$ a\CONFIG_RTE_LIBRTE_KNI=n" $conf
	cmd sed -i -e"$ a\CONFIG_RTE_KNI_KMOD=n" $conf
}

# build dpdk for the given tag and dump abi
# $1: hash of the revision
gen_abi() {
	local i

	cmd git clone ${dpdkroot} ${dst}/${1}
	cmd cd ${dst}/${1}

	log "INFO" "Checking out version ${1} of the dpdk"
	# Move to the old version of the tree
	cmd git checkout ${1}

	fixup_config

	# Now configure the build
	log "INFO" "Configuring DPDK ${1}"
	cmd make config T=$target O=$target

	# Checking abi compliance relies on using the dwarf information in
	# the shared objects. Build with -g to include them.
	log "INFO" "Building DPDK ${1}. This might take a moment"
	cmd make -j$parallel O=$target V=1 EXTRA_CFLAGS="-g -Og -Wno-error" \
	    EXTRA_LDFLAGS="-g" || log "INFO" "The build failed"

	# Move to the lib directory
	cmd cd ${PWD}/$target/lib
	log "INFO" "Collecting ABI information for ${1}"
	for i in *.so; do
		[ -e "$i" ] || break
		cmd $abidump ${i} -o $dst/${1}/${i}.dump -lver ${1} || true
		# hack to ignore empty SymbolsInfo section (no public ABI)
		if grep -q "'SymbolInfo' => {}," $dst/${1}/${i}.dump \
				2> /dev/null; then
			log "INFO" "${i} has no public ABI, remove dump file"
			cmd rm -f $dst/${1}/${i}.dump
		fi
	done
}

verbose=false
parallel=1
dst=${default_dst}
target=${default_target}
force=0
while getopts j:vd:t:fh ARG ; do
	case $ARG in
		j ) parallel=$OPTARG ;;
		v ) verbose=true ;;
		d ) dst=$OPTARG ;;
		t ) target=$OPTARG ;;
		f ) force=1 ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

if [ $# != 2 ]; then
	print_usage
	exit 1
fi

tag1=$1
tag2=$2

# convert path to absolute
case "${dst}" in
	/*) ;;
	*) dst=${PWD}/${dst} ;;
esac
dpdkroot=$(readlink -e $(dirname $0)/..)

if [ -e "${dst}" -a "$force" = 0 ]; then
	echo "The ${dst} directory is not empty. Remove it, use another"
	echo "one (-d <dir>), or force overriding (-f)"
	exit 1
fi

rm -rf ${dst}
mkdir -p ${dst}
set_log_file ${dst}/abi-check.log
log "INFO" "Logs available in ${dst}/abi-check.log"

command -v ${abicheck} || log "INFO" "Can't find ${abicheck} utility"
command -v ${abidump} || log "INFO" "Can't find ${abidump} utility"

hash1=$(git show -s --format=%h "$tag1" -- 2> /dev/null | tail -1)
hash2=$(git show -s --format=%h "$tag2" -- 2> /dev/null | tail -1)

# Make hashes available in output for non-local reference
tag1="$tag1 ($hash1)"
tag2="$tag2 ($hash2)"

if [ "$hash1" = "$hash2" ]; then
	log "ERROR" "$tag1 and $tag2 are the same revisions"
	exit 1
fi

cmd mkdir -p ${dst}

# dump abi for each revision
gen_abi ${hash1}
gen_abi ${hash2}

# compare the abi dumps
cmd cd ${dst}
ret=0
list=""
for i in ${hash2}/*.dump; do
	name=`basename $i`
	libname=${name%.dump}

	if [ ! -f ${hash1}/$name ]; then
		log "INFO" "$NAME does not exist in $tag1. skipping..."
		continue
	fi

	local_ret=0
	cmd $abicheck -l $libname \
	    -old ${hash1}/$name -new ${hash2}/$name || local_ret=$?
	if [ $local_ret != 0 ]; then
		log "NOTICE" "$abicheck returned $local_ret"
		ret=$local_ret
		list="$list $libname"
	fi
done

if [ $ret != 0 ]; then
	log "NOTICE" "ABI may be incompatible, check reports/logs for details."
	log "NOTICE" "Incompatible list: $list"
else
	log "NOTICE" "No error detected, ABI is compatible."
fi

log "INFO" "Logs are in ${dst}/abi-check.log"
log "INFO" "HTML reports are in ${dst}/compat_reports directory"

exit $ret
