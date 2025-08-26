#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2024 Red Hat, Inc.

#
# Import and check Linux Kernel uAPI headers
#

base_url="https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/include/uapi/"
base_path="kernel/linux/uapi/"
version=""
file=""
check_headers=0
errors=0

print_usage()
{
	echo "Usage: $(basename $0) [-h] [-i FILE] [-u VERSION] [-c]"
	echo "-i FILE      import Linux header file. E.g. linux/vfio.h"
	echo "-u VERSION   update imported list of Linux headers to a given version. E.g. v6.10"
	echo "-c           check headers are valid"
}

version_older_than() {
    printf '%s\n%s' "$1" "$2" | sort -C -V
}

download_header()
{
	local header=$1
	local path=$2

	local url="$base_url$header?h=$version"

	if ! curl -s -f --create-dirs -o $path $url; then
		echo "Failed to download $url"
		return 1
	fi

	return 0
}

update_headers()
{
	local header
	local url
	local path

	echo "Updating to $version"
	for filename in $(find $base_path -name "*.h" -type f); do
		header=${filename#$base_path}
		download_header $header $filename
	done

	return 0
}

import_header()
{
	local include
	local import
	local header=$1

	local path="$base_path$header"

	download_header $header $path

	for include in $(sed -ne 's/^#include <\(.*\)>$/\1/p' $path); do
		if [ ! -f "$base_path$include" ]; then
			read -p "Import $include (y/n): " import && [ "$import" = 'y' ] || continue
			echo "Importing $include for $path"
			import_header "$include"
		fi
	done

	return 0
}

fixup_includes()
{
	local path=$1

	sed -i "s|^#include <linux/compiler.h>||g" $path
	sed -i "s|\<__user[[:space:]]||" $path

	# Prepend include path with "uapi/" if the header is imported
	for include in $(sed -ne 's/^#include <\(.*\)>$/\1/p' $path); do
		if [ -f "$base_path$include" ]; then
			sed -i "s|$include|uapi/$include|g" $path
		fi
	done
}

check_header() {
	echo -n "Checking $1... "

	if ! diff -q $1 $2 >/dev/null; then
		echo "KO"
		diff -u $1 $2
		return 1
	else
		echo "OK"
	fi

	return 0
}

while getopts i:u:ch opt ; do
	case $opt in
		i ) file=$OPTARG ;;
		u ) version=$OPTARG ;;
		c ) check_headers=1 ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done

shift $(($OPTIND - 1))
if [ $# -ne 0 ]; then
	print_usage
	exit 1
fi

cd $(dirname $0)/..

current_version=$(< $base_path/version)

if [ -n "$version" ]; then
	if version_older_than "$version" "$current_version"; then
		echo "Headers already up to date ($current_version >= $version)"
		version=$current_version
	else
		update_headers
	fi
else
	echo "Version not specified, using current version ($current_version)"
	version=$current_version
fi

if [ -n "$file" ]; then
	import_header $file
fi

for filename in $(find $base_path -name "*.h" -type f); do
	fixup_includes $filename
done

echo $version > $base_path/version

if [ $check_headers -eq 0 ]; then
	exit 0
fi

tmpheader="$(mktemp -t dpdk.checkuapi.XXXXXX)"
trap "rm -f '$tmpheader'" INT

echo "Checking imported headers for version $version"

for filename in $(find $base_path -name "*.h" -type f); do
	header=${filename#$base_path}
	download_header $header $tmpheader
	fixup_includes $tmpheader
	check_header $filename $tmpheader || errors=$((errors+1))
done

echo "$errors error(s) found"

rm -f $tmpheader
trap - INT

exit $errors
