#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

# Load config options:
# - DPDK_CHECKPATCH_PATH
# - DPDK_CHECKPATCH_CODESPELL
# - DPDK_CHECKPATCH_LINE_LENGTH
# - DPDK_CHECKPATCH_OPTIONS
. $(dirname $(readlink -f $0))/load-devel-config

VALIDATE_NEW_API=$(dirname $(readlink -f $0))/check-symbol-change.py

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
NEW_TYPEDEFS,COMPARISON_TO_NULL,AVOID_BUG,EXPORT_SYMBOL"
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

	# refrain from new calls to RTE_LOG in libraries
	awk -v FOLDERS="lib" \
		-v EXPRESSIONS="RTE_LOG\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Prefer RTE_LOG_LINE' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from new calls to RTE_LOG in drivers (but leave some leeway for base drivers)
	awk -v FOLDERS="drivers" \
		-v SKIP_FILES='.*osdep.h$' \
		-v EXPRESSIONS="RTE_LOG\\\( RTE_LOG_DP\\\( rte_log\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Prefer RTE_LOG_LINE/RTE_LOG_DP_LINE' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# no output on stdout or stderr
	awk -v FOLDERS="lib drivers" \
		-v EXPRESSIONS="\\\<printf\\\> \\\<fprintf\\\(stdout, \\\<fprintf\\\(stderr," \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Writing to stdout or stderr' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

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
		-v SKIP_FILES='lib/eal/include/rte_common.h' \
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

	# refrain from using compiler __rte_atomic_thread_fence()
	# It should be avoided on x86 for SMP case.
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="__rte_atomic_thread_fence\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __rte_atomic_thread_fence, prefer rte_atomic_thread_fence' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using compiler __atomic_xxx builtins
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="__atomic_.*\\\( __ATOMIC_(RELAXED|CONSUME|ACQUIRE|RELEASE|ACQ_REL|SEQ_CST)" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __atomic_xxx/__ATOMIC_XXX built-ins, prefer rte_atomic_xxx/rte_memory_order_xxx' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# refrain from using some pthread functions
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS="pthread_(create|join|detach|set(_?name_np|affinity_np)|attr_set(inheritsched|schedpolicy))\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using pthread functions, prefer rte_thread' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of __reserved which is a reserved keyword in Windows system headers
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS='\\<__reserved\\>' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __reserved' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of __alignof__
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS='\\<__alignof__\\>' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __alignof__, prefer C11 alignof' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of __typeof__
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS='\\<__typeof__\\>' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __typeof__, prefer typeof' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of compiler __builtin_*
	awk -v FOLDERS="lib drivers app examples" \
		-v SKIP_FILES='lib/eal/ drivers/.*/base/ drivers/.*osdep.h$' \
		-v EXPRESSIONS='\\<__builtin_' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __builtin helpers, prefer EAL macros' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid inclusion of Linux header for PCI constants
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS='include.*linux/pci_regs\\.h' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using linux/pci_regs.h, prefer rte_pci.h' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of variadic argument pack extension in macros
	awk -v FOLDERS="lib drivers app examples" \
		-v EXPRESSIONS='#[[:space:]]*define.*[^(,[:space:]]\\.\\.\\.[[:space:]]*)' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Do not use variadic argument pack in macros' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid use of __rte_packed_begin with enums
	awk -v FOLDERS='lib drivers app examples' \
		-v EXPRESSIONS='enum.*__rte_packed_begin' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using __rte_packed_begin with enum is not allowed' \
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

	# forbid non-internal thread in drivers and libs
	awk -v FOLDERS='lib drivers' \
		-v EXPRESSIONS="rte_thread_(set_name|create_control)\\\(" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Prefer rte_thread_(set_prefixed_name|create_internal_control)' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid rte_ symbols in cnxk base driver
	awk -v FOLDERS='drivers/common/cnxk/roc_*' \
		-v SKIP_FILES='.*roc_platform.*' \
		-v EXPRESSIONS="rte_ RTE_" \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Use plt_ symbols instead of rte_ API in cnxk base driver' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# forbid inclusion of driver specific headers in apps and examples
	awk -v FOLDERS='app examples' \
		-v EXPRESSIONS='include.*_driver\\.h include.*_pmd\\.h' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using driver specific headers in applications' \
		-f $(dirname $(readlink -f $0))/check-forbidden-tokens.awk \
		"$1" || res=1

	# prevent addition of tests not in one of our test suites
	awk -v FOLDERS='app/test' \
		-v EXPRESSIONS='REGISTER_TEST_COMMAND' \
		-v RET_ON_FAIL=1 \
		-v MESSAGE='Using REGISTER_TEST_COMMAND instead of REGISTER_<suite_name>_TEST' \
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

check_aligned_attributes() { # <patch>
	res=0

	for token in __rte_aligned __rte_cache_aligned __rte_cache_min_aligned; do
		if [ $(grep -E '^\+.*\<'$token'\>' "$1" | \
				grep -vE '\<(struct|union)[[:space:]]*'$token'\>' | \
				wc -l) != 0 ]; then
			echo "Please use $token only for struct or union types alignment."
			res=1
		fi
	done

	return $res
}

check_packed_attributes() { # <patch>
	res=0

	if [ $(grep -E '^\+.*__rte_packed_begin' "$1" | \
			grep -vE '\<struct[[:space:]]*__rte_packed_begin\>' | \
			grep -vE '\<union[[:space:]]*__rte_packed_begin\>' | \
			grep -vE '\<__rte_cache_aligned[[:space:]]*__rte_packed_begin\>' | \
			grep -vE '\<__rte_cache_min_aligned[[:space:]]*__rte_packed_begin\>' | \
			grep -vE '\<__rte_aligned\(.*\)[[:space:]]*__rte_packed_begin\>' | \
			wc -l) != 0 ]; then
		echo "Use __rte_packed_begin only after struct, union or alignment attributes."
		res=1
	fi

	begin_count=$(grep '__rte_packed_begin' "$1" | wc -l)
	end_count=$(grep '__rte_packed_end' "$1" | wc -l)
	if [ $begin_count != $end_count ]; then
		echo "__rte_packed_begin and __rte_packed_end should always be used in pairs."
		res=1
	fi

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
	report=$($VALIDATE_NEW_API --patch "$tmpinput")
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

	! $verbose || printf '\nChecking alignment attributes:\n'
	report=$(check_aligned_attributes "$tmpinput")
	if [ $? -ne 0 ] ; then
		$headline_printed || print_headline "$subject"
		printf '%s\n' "$report"
		ret=1
	fi

	! $verbose || printf '\nChecking packed attributes:\n'
	report=$(check_packed_attributes "$tmpinput")
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
