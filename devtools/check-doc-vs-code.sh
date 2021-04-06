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
	test -n "$(git log --format='%h' -S"$1" $trusted_commit..)"
}

has_file_change() # <pattern>
{
	changed_files | grep -q "$1"
}

changed_net_drivers()
{
	net_paths='drivers/net/|doc/guides/nics/features/'
	[ -n "$drivers" ] ||
		drivers=$(changed_files |
			sed -rn "s,^($net_paths)([^./]*).*,\2,p" |
			sort -u)
	echo "$drivers"
}

all_net_drivers()
{
	find $rootdir/drivers/net -mindepth 1 -maxdepth 1 -type d |
	sed 's,.*/,,' |
	sort
}

check_rte_flow() # <driver>
{
	code=$rootdir/drivers/net/$1
	doc=$rootdir/doc/guides/nics/features/$1.ini
	[ -d $code ] || return 0
	[ -f $doc ] || return 0
	report=$($selfdir/parse-flow-support.sh $code $doc)
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
