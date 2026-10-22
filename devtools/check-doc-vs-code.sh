#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021 Mellanox Technologies, Ltd

# Check whether doc & code are in sync.
# Optional argument: check only what changed since a commit.
trusted_commit=$1 # example: origin/main

selfdir=$(dirname $(readlink -f $0))
rootdir=$(readlink -f $selfdir/..)

# speed up by ignoring Unicode details
export LC_COLLATE=C

result=0
error() # <message>
{
	echo "$*"
	result=$(($result + 1))
}

changed_files()
{
	[ -n "$files" ] ||
		files=$(git diff-tree --name-only -r $trusted_commit..)
	echo "$files"
}

has_code_change() # <pattern>
{
	test -n "$(git log --format='%h' -G"$1" $trusted_commit..)"
}

has_file_change() # <pattern>
{
	changed_files | grep -q "$1"
}

net_driver_from_path() # <path>
{
	sed -rn '
		s,.*drivers/net/intel/([^/]*)/.*,\1,p; t
		s,.*drivers/net/([^./]*)/.*,\1,p; t
		s,.*doc/guides/nics/features/([^./]*).*,\1,p' |
	grep -v '^default$'
}

changed_net_drivers()
{
	[ -n "$drivers" ] ||
		drivers=$(changed_files |
			net_driver_from_path |
			sort -u)
	echo "$drivers"
}

all_net_drivers()
{
	{
		find $rootdir/drivers/net -name meson.build
		ls $rootdir/doc/guides/nics/features/*.ini
	} |
	net_driver_from_path |
	sort -u
}

# give back a driver directory for a doc/driver name
driver_code_dir() # <driver_name>
{
	drv=$(echo "$1" | sed -r 's/_(dcf|vec|vf)$//')
	case $drv in
	enetc|enetc4)
		echo "$rootdir/drivers/net/enetc" ;;
	igb|igc|e1000)
		echo "$rootdir/drivers/net/intel/e1000" ;;
	*)
		if [ -d $rootdir/drivers/net/intel/$drv ]; then
			echo "$rootdir/drivers/net/intel/$drv"
		else
			echo "$rootdir/drivers/net/$drv"
		fi ;;
	esac
}

# create a cache of the mapping between a driver code directory and associated driver docs
# format of this cache is:
# ^,$drv1_directory: $drv1_doc_name1 $drv1_doc_name2,$drv2_directory: $drv2_doc_name1,...,$
doc_code_map=","
for d in $rootdir/doc/guides/nics/features/*.ini; do
	drv=$(basename "$d" .ini)
	[ "$drv" != 'default' ] || continue
	code=$(driver_code_dir $drv)
	case $doc_code_map in
	*,$code:*)
		before=${doc_code_map%,$code:*}
		after=${doc_code_map#*,$code:}
		drvs=${after%%,*}
		rest=${after#*,}
		doc_code_map="$before,$code:$drvs $drv,$rest" ;;
	*)
		doc_code_map="$doc_code_map$code: $drv," ;;
	esac
done

# extract all doc names from the cache created above
docs_for_code() # <code_dir>
{
	tmp=${doc_code_map#*,$1:}
	echo ${tmp%%,*}
}

# output items from first list that are not in second list
items_not_in() # <list1> <list2>
{
	{
		[ -n "$1" ] && echo "$1"
		[ -n "$2" ] && { echo "$2"; echo "$2"; }
	} | sort | uniq -u
}

check_rte_flow() # <driver>
{
	code=$(driver_code_dir $1)
	doc=$rootdir/doc/guides/nics/features/$1.ini
	[ -d $code ] || return 0
	[ -f $doc ] || return 0
	code_support=$($selfdir/parse-flow-support.sh $code)
	report=$(
		for category in item action; do
			section="rte_flow ${category}s]"
			code_items=$(echo "$code_support" | sed -n "/$section/,/]/p" |
				sed '/]/d' |        # ignore section title
				sed 's, *=.*,,' |   # keep only item name
				sed '/^$/d' |       # remove empty lines
				sort -u)
			doc_items=$(sed -n "/$section/,/]/p" "$doc" |
				sed '/]/d' |        # ignore section title
				sed 's, *=.*,,' |   # keep only item name
				sed '/^$/d' |       # remove empty lines
				sort -u)
			{
				# items documented but not in code
				items_not_in "$doc_items" "$code_items"
				# items in code but not documented in any doc for the code directory
				all_doc_items=$(
					for drv in $(docs_for_code $code); do
						d=$rootdir/doc/guides/nics/features/$drv.ini
						sed -n "/$section/,/]/p" "$d" |
							sed '/]/d' |        # ignore section title
							sed 's, *=.*,,' |   # keep only item name
							sed '/^$/d'         # remove empty lines
					done | sort -u
				)
				items_not_in "$code_items" "$all_doc_items"
			} | sed "s,^,$category ,"   # prefix with category name
		done
	)
	if [ -n "$report" ]; then
		error "rte_flow doc out of sync for $1"
		echo "$report" | sed 's,^,\t,'
	fi
}

if [ -z "$trusted_commit" ]; then
	# check all
	for driver in $(all_net_drivers); do
		check_rte_flow $driver
	done
	exit $result
fi

# find what changed and check
if has_code_change 'RTE_FLOW_.*_TYPE_' ||
		has_file_change 'doc/guides/nics/features'; then
	for driver in $(changed_net_drivers); do
		check_rte_flow $driver
	done
fi
exit $result
