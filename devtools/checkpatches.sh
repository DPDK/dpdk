#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

# Load config options:
# - DPDK_CHECKPATCH_PATH
# - DPDK_CHECKPATCH_LINE_LENGTH
. $(dirname $(readlink -e $0))/load-devel-config

length=${DPDK_CHECKPATCH_LINE_LENGTH:-80}

# override default Linux options
options="--no-tree"
options="$options --max-line-length=$length"
options="$options --show-types"
options="$options --ignore=LINUX_VERSION_CODE,\
FILE_PATH_CHANGES,MAINTAINERS_STYLE,\
VOLATILE,PREFER_PACKED,PREFER_ALIGNED,PREFER_PRINTF,\
PREFER_KERNEL_TYPES,BIT_MACRO,CONST_STRUCT,\
SPLIT_STRING,LONG_LINE_STRING,\
LINE_SPACING,PARENTHESIS_ALIGNMENT,NETWORKING_BLOCK_COMMENT_STYLE,\
NEW_TYPEDEFS,COMPARISON_TO_NULL"

print_usage () {
	cat <<- END_OF_HELP
	usage: $(basename $0) [-q] [-v] [-nX|patch1 [patch2] ...]]

	Run Linux kernel checkpatch.pl with DPDK options.
	The environment variable DPDK_CHECKPATCH_PATH must be set.

	The patches to check can be from stdin, files specified on the command line,
	or latest git commits limited with -n option (default limit: origin/master).
	END_OF_HELP
}

number=0
quiet=false
verbose=false
while getopts hn:qv ARG ; do
	case $ARG in
		n ) number=$OPTARG ;;
		q ) quiet=true ;;
		v ) verbose=true ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

if [ ! -f "$DPDK_CHECKPATCH_PATH" ] || [ ! -x "$DPDK_CHECKPATCH_PATH" ] ; then
	print_usage >&2
	echo
	echo 'Cannot execute DPDK_CHECKPATCH_PATH' >&2
	exit 1
fi

total=0
status=0

check () { # <patch> <commit> <title>
	total=$(($total + 1))
	! $verbose || printf '\n### %s\n\n' "$3"
	if [ -n "$1" ] ; then
		report=$($DPDK_CHECKPATCH_PATH $options "$1" 2>/dev/null)
	elif [ -n "$2" ] ; then
		report=$(git format-patch --find-renames --no-stat --stdout -1 $commit |
			$DPDK_CHECKPATCH_PATH $options - 2>/dev/null)
	else
		report=$($DPDK_CHECKPATCH_PATH $options - 2>/dev/null)
	fi
	[ $? -ne 0 ] || return 0
	$verbose || printf '\n### %s\n\n' "$3"
	printf '%s\n' "$report" | sed -n '1,/^total:.*lines checked$/p'
	status=$(($status + 1))
}

if [ -n "$1" ] ; then
	for patch in "$@" ; do
		# Subject can be on 2 lines
		subject=$(sed '/^Subject: */!d;s///;N;s,\n[[:space:]]\+, ,;s,\n.*,,;q' "$patch")
		check "$patch" '' "$subject"
	done
elif [ ! -t 0 ] ; then # stdin
	subject=$(while read header value ; do
		if [ "$header" = 'Subject:' ] ; then
			IFS= read next
			continuation=$(echo "$next" | sed -n 's,^[[:space:]]\+, ,p')
			echo $value$continuation
			break
		fi
	done)
	check '' '' "$subject"
else
	if [ $number -eq 0 ] ; then
		commits=$(git rev-list --reverse origin/master..)
	else
		commits=$(git rev-list --reverse --max-count=$number HEAD)
	fi
	for commit in $commits ; do
		subject=$(git log --format='%s' -1 $commit)
		check '' $commit "$subject"
	done
fi
pass=$(($total - $status))
$quiet || printf '\n%d/%d valid patch' $pass $total
$quiet || [ $pass -le 1 ] || printf 'es'
$quiet || printf '\n'
exit $status
