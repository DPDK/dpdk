#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021 Mellanox Technologies, Ltd

# Parse rte_flow support of a driver directory,
# and optionally show difference with a doc file in .ini format.

dir=$1 # drivers/net/foo
ref=$2 # doc/guides/nics/features/foo.ini

if [ -z "$dir" ]; then
	echo "directory argument is required" >&2
	exit 1
fi

# test git-grep for -o (--only-matching) option
if ! git grep -qo git -- $0 >/dev/null 2>&1; then
	echo "git version >= 2.19 is required" >&2
	exit 1
fi

# sorting order
export LC_COLLATE=C

# exclude exceptions
exclude() # <pattern>
{
	case $(basename $dir) in
		bnxt)
			filter=$(sed -n "/$1/{N;/TYPE_NOT_SUPPORTED/P;}" \
				$dir/tf_ulp/ulp_rte_handler_tbl.c |
				grep -wo "$1[[:alnum:]_]*" | sort -u |
				tr '\n' '|' | sed 's,.$,\n,')
			exceptions='RTE_FLOW_ACTION_TYPE_SHARED'
			grep -vE "$filter" | grep -vE $exceptions;;
		*) cat
	esac
}

# include exceptions
include() # <pattern>
{
	case $(basename $dir) in
	esac
}

# generate INI section
list() # <title> <pattern>
{
	echo "[$1]"
	git grep -who "$2[[:alnum:]_]*" $dir |
	(exclude $2; include $2) | sort -u |
	awk 'sub(/'$2'/, "") {printf "%-20s = Y\n", tolower($0)}'
}

rte_flow_support() # <category>
{
	title="rte_flow $1s"
	pattern=$(echo "RTE_FLOW_$1_TYPE_" | awk '{print toupper($0)}')
	list "$title" "$pattern" | grep -vwE 'void|indirect|end'
}

if [ -z "$ref" ]; then # generate full tables
	rte_flow_support item
	echo
	rte_flow_support action
	exit 0
fi

# compare with reference input
rte_flow_compare() # <category>
{
	section="rte_flow $1s]"
	{
		rte_flow_support $1
		sed -n "/$section/,/]/p" "$ref" | sed '/^$/d'
	} |
	sed '/]/d' | # ignore section title
	sed 's, *=.*,,' | # ignore value (better in doc than generated one)
	sort | uniq -u | # show differences
	sed "s,^,$1 ," # prefix with category name
}

rte_flow_compare item
rte_flow_compare action
