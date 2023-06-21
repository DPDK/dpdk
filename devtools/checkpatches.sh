#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

# Load config options:
# - DPDK_CHECKPATCH_PATH
# - DPDK_CHECKPATCH_CODESPELL
# - DPDK_CHECKPATCH_LINE_LENGTH
# - DPDK_CHECKPATCH_OPTIONS
. $(dirname $(readlink -f $0))/load-devel-config

VALIDATE_NEW_API=$(dirname $(readlink -f $0))/check-symbol-change.sh

# Enable codespell by default. This can be overwritten from a config file.
# Codespell can also be enabled by setting DPDK_CHECKPATCH_CODESPELL to a valid path
# to a dictionary.txt file if dictionary.txt is not in the default location.
codespell=${DPDK_CHECKPATCH_CODESPELL:-enable}
length=${DPDK_CHECKPATCH_LINE_LENGTH:-100}

# override default Linux options
options="--no-tree"
if [ "$codespell" = "enable" ] ; then
    options="$options --codespell"
elif [ -f "$codespell" ] ; then
    options="$options --codespell"
    options="$options --codespellfile $codespell"
fi
options="$options --max-line-length=$length"
options="$options --show-types"
options="$options --ignore=LINUX_VERSION_CODE,ENOSYS,\
FILE_PATH_CHANGES,MAINTAINERS_STYLE,SPDX_LICENSE_TAG,\
VOLATILE,PREFER_PACKED,PREFER_ALIGNED,PREFER_PRINTF,STRLCPY,\
PREFER_KERNEL_TYPES,PREFER_FALLTHROUGH,BIT_MACRO,CONST_STRUCT,\
SPLIT_STRING,LONG_LINE_STRING,C99_COMMENT_TOLERANCE,\
LINE_SPACING,PARENTHESIS_ALIGNMENT,NETWORKING_BLOCK_COMMENT_STYLE,\
NEW_TYPEDEFS,COMPARISON_TO_NULL,AVOID_BUG"
options="$options $DPDK_CHECKPATCH_OPTIONS"

print_usage () {
	cat <<- END_OF_HELP
	usage: $(basename $0) [-h] [-q] [-v] [-nX|-r range|patch1 [patch2] ...]

	Run Linux kernel checkpatch.pl with DPDK options.
	The environment variable DPDK_CHECKPATCH_PATH can be set, if not we will
	try to find the script in the sources of the currently running kernel.

	The patches to check can be from stdin, files specified on the command line,
	latest git commits limited with -n option, or commits in the git range
	specified with -r option (default: "origin/main..").
	END_OF_HELP
}

check_forbidden_additions() { # <patch>
	res=0

	# refrain from new additions of rte_panic() and rte_exit()
	# multiple folders and expressions are separated by spaces
	awk -v FOLDERS="lib drivers" \
		-v EXPRESSIONS="rte_panic\\\( rte_exit\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using rte_panic/rte_exit' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using compiler attribute without defining a common macro
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="__attribute__" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using compiler attribute directly' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# check %l or %ll format specifier
	awk -v FOLDERS='lib drivers app examples' \
		-v EXPRESSIONS='%ll*[xud]' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using %l format, prefer %PRI*64 if type is [u]int64_t' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from new additions of 16/32/64 bits rte_atomicNN_xxx()
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="rte_atomic[0-9][0-9]_.*\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using rte_atomicNN_xxx' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from new additions of rte_smp_[r/w]mb()
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="rte_smp_(r|w)?mb\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using rte_smp_[r/w]mb' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using compiler __sync_xxx builtins
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="__sync_.*\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __sync_xxx builtins' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using compiler __atomic_thread_fence()
	# It should be avoided on x86 for SMP case.
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="__atomic_thread_fence\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __atomic_thread_fence' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using compiler __atomic_{add,and,nand,or,sub,xor}_fetch()
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="__atomic_(add|and|nand|or|sub|xor)_fetch\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __atomic_op_fetch, prefer __atomic_fetch_op' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of __reserved which is a reserved keyword in Windows system headers
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS='\\<__reserved\\>' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __reserved' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of experimental build flag except in examples
	awk -v FOLDERS='lib drivers app' \
		-v EXPRESSIONS='-DALLOW_EXPERIMENTAL_API allow_experimental_apis' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using experimental build flag for in-tree compilation' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using RTE_LOG_REGISTER for drivers and libs
	awk -v FOLDERS='lib drivers' \
		-v EXPRESSIONS='\\<RTE_LOG_REGISTER\\>' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using RTE_LOG_REGISTER, prefer RTE_LOG_REGISTER_(DEFAULT|SUFFIX)' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid inclusion of driver specific headers in apps and examples
	awk -v FOLDERS='app examples' \
		-v EXPRESSIONS='include.*_driver\\.h include.*_pmd\\.h' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using driver specific headers in applications' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# SVG must be included with wildcard extension to allow conversion
	awk -v FOLDERS='doc' \
		-v EXPRESSIONS='::[[:space:]]*[^[:space:]]*\\.svg' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using explicit .svg extension instead of .*' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# links must prefer https over http
	awk -v FOLDERS='doc' \
		-v EXPRESSIONS='http://.*dpdk.org' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using non https link to dpdk.org' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# prefer Sphinx references for internal documentation
	awk -v FOLDERS='doc' \
		-v EXPRESSIONS='//doc.dpdk.org/guides/' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using explicit URL to doc.dpdk.org, prefer :ref: or :doc:' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# '// XXX is not set' must be preferred over '#undef XXX'
	awk -v FOLDERS='config/rte_config.h' \
		-v EXPRESSIONS='#undef' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using "#undef XXX", prefer "// XXX is not set"' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	return $res
}

check_experimental_tags() { # <patch>
	res=0

	cat "$1" |awk '
	BEGIN {
		current_file = "";
		ret = 0;
	}
	/^+++ b\// {
		current_file = $2;
	}
	/^+.*__rte_experimental/ {
		if (current_file ~ ".c$" ) {
			print "Please only put __rte_experimental tags in " \
				"headers ("current_file")";
			ret = 1;
		}
		if ($1 != "+__rte_experimental" || $2 != "") {
			print "__rte_experimental must appear alone on the line" \
				" immediately preceding the return type of a function."
			ret = 1;
		}
	}
	END {
		exit ret;
	}' || res=1

	return $res
}

check_internal_tags() { # <patch>
	res=0

	cat "$1" |awk '
	BEGIN {
		current_file = "";
		ret = 0;
	}
	/^+++ b\// {
		current_file = $2;
	}
	/^+.*__rte_internal/ {
		if (current_file ~ ".c$" ) {
			print "Please only put __rte_internal tags in " \
				"headers ("current_file")";
			ret = 1;
		}
		if ($1 != "+__rte_internal" || $2 != "") {
			print "__rte_internal must appear alone on the line" \
				" immediately preceding the return type of" \
				" a function."
			ret = 1;
		}
	}
	END {
		exit ret;
	}' || res=1

	return $res
}

check_release_notes() { # <patch>
	rel_notes_prefix=doc/guides/rel_notes/release_
	IFS=. read year month release < VERSION
	current_rel_notes=${rel_notes_prefix}${year}_${month}.rst

	! grep -e '^--- a/'$rel_notes_prefix -e '^+++ b/'$rel_notes_prefix "$1" |
		grep -v $current_rel_notes
}

number=0
range='origin/main..'
quiet=false
verbose=false
while getopts hn:qr:v ARG ; do
	case $ARG in
		n ) number=$OPTARG ;;
		q ) quiet=true ;;
		r ) range=$OPTARG ;;
		v ) verbose=true ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

if [ ! -f "$DPDK_CHECKPATCH_PATH" ] || [ ! -x "$DPDK_CHECKPATCH_PATH" ] ; then
	default_path="/lib/modules/$(uname -r)/source/scripts/checkpatch.pl"
	if [ -f "$default_path" ] && [ -x "$default_path" ] ; then
		DPDK_CHECKPATCH_PATH="$default_path"
	else
		print_usage >&2
		echo
		echo 'Cannot execute DPDK_CHECKPATCH_PATH' >&2
		exit 1
	fi
fi

print_headline() { # <title>
	printf '\n### %s\n\n' "$1"
	headline_printed=true
}

total=0
status=0

check () { # <patch-file> <commit>
	local ret=0
	local subject=''
	headline_printed=false

	total=$(($total + 1))
	if [ -n "$1" ] ; then
		tmpinput=$1
	else
		tmpinput=$(mktemp -t dpdk.checkpatches.XXXXXX)
		trap "rm -f '$tmpinput'" INT

		if [ -n "$2" ] ; then
			git format-patch --find-renames \
			--no-stat --stdout -1 $commit > "$tmpinput"
		else
			cat > "$tmpinput"
		fi
	fi

	# Subject can be on 2 lines
	subject=$(sed '/^Subject: */!d;s///;N;s,\n[[:space:]]\+, ,;s,\n.*,,;q' "$tmpinput")
	! $verbose || print_headline "$subject"

	! $verbose || printf 'Running checkpatch.pl:\n'
	report=$($DPDK_CHECKPATCH_PATH $options "$tmpinput" 2>/dev/null)
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report" | sed -n '1,/^total:.*lines checked$/p'
		ret=1
	fi

	! $verbose || printf '\nChecking API additions/removals:\n'
	report=$($VALIDATE_NEW_API "$tmpinput")
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report"
		ret=1
	fi

	! $verbose || printf '\nChecking forbidden tokens additions:\n'
	report=$(check_forbidden_additions "$tmpinput")
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report"
		ret=1
	fi

	! $verbose || printf '\nChecking __rte_experimental tags:\n'
	report=$(check_experimental_tags "$tmpinput")
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report"
		ret=1
	fi

	! $verbose || printf '\nChecking __rte_internal tags:\n'
	report=$(check_internal_tags "$tmpinput")
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report"
		ret=1
	fi

	! $verbose || printf '\nChecking release notes updates:\n'
	report=$(check_release_notes "$tmpinput")
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report"
		ret=1
	fi

	if [ "$tmpinput" != "$1" ]; then
		rm -f "$tmpinput"
		trap - INT
	fi
	[ $ret -eq 0 ] && return 0

	status=$(($status + 1))
}

if [ -n "$1" ] ; then
	for patch in "$@" ; do
		check "$patch" ''
	done
elif [ ! -t 0 ] ; then # stdin
	check '' ''
else
	if [ $number -eq 0 ] ; then
		commits=$(git rev-list --reverse $range)
	else
		commits=$(git rev-list --reverse --max-count=$number HEAD)
	fi
	for commit in $commits ; do
		check '' $commit
	done
fi
pass=$(($total - $status))
$quiet || printf '\n%d/%d valid patch' $pass $total
$quiet || [ $pass -le 1 ] || printf 'es'
$quiet || printf '\n'
exit $status
