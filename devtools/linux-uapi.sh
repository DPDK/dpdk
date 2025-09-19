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
check_headers=false
quiet=false

print_usage()
{
	echo "Usage: $(basename $0) [-h] [-i FILE] [-u VERSION] [-c] [-q]"
	echo "-i FILE      import Linux header file. E.g. linux/vfio.h"
	echo "-u VERSION   update imported list of Linux headers to a given version. E.g. v6.10"
	echo "-c           check headers are valid"
	echo "-q           quiet mode"
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

	$quiet || echo "Updating to $version"
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
			$quiet || echo "Importing $include for $path"
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
	sed -i 's|#\(ifndef\)[[:space:]]*_UAPI|#\1 |' $path
	sed -i 's|#\(define\)[[:space:]]*_UAPI|#\1 |' $path
	sed -i 's|#\(endif[[:space:]]*/[*]\)[[:space:]]*_UAPI|#\1 |' $path

	# Prepend include path with "uapi/" if the header is imported
	for include in $(sed -ne 's/^#include <\(.*\)>$/\1/p' $path); do
		if [ -f "$base_path$include" ]; then
			sed -i "s|$include|uapi/$include|g" $path
		fi
	done
}

update_all()
{
	if [ -n "$version" ]; then
		if version_older_than "$version" "$current_version"; then
			$quiet || echo "Headers already up to date ($current_version >= $version)"
			version=$current_version
		else
			update_headers
		fi
	else
		$quiet || echo "Version not specified, using current version ($current_version)"
		version=$current_version
	fi

	if [ -n "$file" ]; then
		import_header $file
	fi

	for filename in $(find $base_path -name "*.h" -type f); do
		fixup_includes $filename
	done

	echo $version > $base_path/version
}

check_header()
{
	$quiet || echo -n "Checking $1... "

	if ! diff -q $1 $2 >/dev/null; then
		$quiet || echo "KO"
		$quiet || diff -u $1 $2
		return 1
	else
		$quiet || echo "OK"
	fi

	return 0
}

check_all()
{
	local errors=0

	tmpheader="$(mktemp -t dpdk.checkuapi.XXXXXX)"
	trap "rm -f '$tmpheader'" INT

	$quiet || echo "Checking imported headers for version $version"
	for filename in $(find $base_path -name "*.h" -type f); do
		header=${filename#$base_path}
		download_header $header $tmpheader
		fixup_includes $tmpheader
		check_header $filename $tmpheader || errors=$((errors+1))
	done
	if [ $errors -ne 0 ] || ! $quiet; then
		echo "$errors error(s) found in Linux uAPI"
	fi

	rm -f $tmpheader
	trap - INT

	return $errors
}

while getopts i:u:cqh opt ; do
	case $opt in
		i ) file=$OPTARG ;;
		u ) version=$OPTARG ;;
		c ) check_headers=true ;;
		q ) quiet=true ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done

shift $(($OPTIND - 1))
if [ $# -ne 0 ]; then
	print_usage
	exit 1
fi
if $check_headers && [ -n "$file" -o -n "$version" ]; then
	echo "The option -c is incompatible with -i and -u"
	exit 1
fi

cd $(dirname $0)/..
current_version=$(cat $base_path/version)

if $check_headers; then
	version=$current_version
	check_all
else
	update_all
fi
