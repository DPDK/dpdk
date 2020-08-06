#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

# Do some basic checks in MAINTAINERS file

cd $(dirname $0)/..

# speed up by ignoring Unicode details
export LC_ALL=C

# Get files matching paths with wildcards and / meaning recursing
files () # <path> [<path> ...]
{
	if [ -z "$1" ] ; then
		return
	fi
	if [ -d .git ] ; then
		git ls-files "$1"
	else
		find "$1" -type f |
		sed 's,^\./,,'
	fi |
	# if not ended by /
	if ! echo "$1" | grep -q '/[[:space:]]*$' ; then
		# filter out deeper directories
		sed "/\(\/[^/]*\)\{$(($(echo "$1" | grep -o / | wc -l) + 1))\}/d"
	else
		cat
	fi
	# next path
	shift
	files "$@"
}

# Get all files matching F: and X: fields
parse_fx () # <index file>
{
	IFS='
'
	# parse each line excepted underlining
	for line in $( (sed '/^-\+$/d' $1 ; echo) | sed 's,^$,§,') ; do
		if echo "$line" | grep -q '^§$' ; then
			# empty line delimit end of section
			include_files=$(files $flines)
			exclude_files=$(files $xlines)
			match=$(aminusb "$include_files" "$exclude_files")
			if [ -n "$include_files" ] ; then
				printf "# $title "
				maintainers=$(echo "$maintainers" | sed -r 's,.*<(.*)>.*,\1,')
				maintainers=$(printf "$maintainers" | sed -e 's,^,<,' -e 's,$,>,')
				echo $maintainers
			fi
			if [ -n "$match" ] ; then
				echo "$match"
			fi
			# flush section
			unset maintainers
			unset flines
			unset xlines
		elif echo "$line" | grep -q '^[A-Z]: ' ; then
			# maintainer
			maintainers=$(add_line_to_if "$line" "$maintainers" 'M: ')
			# file matching pattern
			flines=$(add_line_to_if "$line" "$flines" 'F: ')
			# file exclusion pattern
			xlines=$(add_line_to_if "$line" "$xlines" 'X: ')
		else # assume it is a title
			title="$line"
		fi
	done
}

# Check patterns in F: and X:
check_fx () # <index file>
{
	IFS='
'
	for line in $(sed -n 's,^[FX]: ,,p' $1 | tr '*' '#') ; do
		line=$(printf "$line" | tr '#' '*')
		match=$(files "$line")
		if [ -z "$match" ] ; then
			echo "$line"
		fi
	done
}

# Add a line to a set of lines if it begins with right pattern
add_line_to_if () # <new line> <lines> <head pattern>
{
	(
		echo "$2"
		echo "$1" | sed -rn "s,^$3(.*),\1,p"
	) |
	sed '/^$/d'
}

# Subtract two sets of lines
aminusb () # <lines a> <lines b>
{
	printf "$1\n$2\n$2" | sort | uniq -u | sed '/^$/d'
}

printf 'sections: '
parsed=$(parse_fx MAINTAINERS)
echo "$parsed" | grep -c '^#'
printf 'with maintainer: '
echo "$parsed" | grep -c '^#.*@'
printf 'maintainers: '
grep '^M:.*<' MAINTAINERS | sort -u | wc -l

echo
echo '##########'
echo '# orphan areas'
echo '##########'
echo "$parsed" | sed -rn 's,^#([^@]*)$,\1,p' | uniq

echo
echo '##########'
echo '# files not listed'
echo '##########'
all=$(files ./)
listed=$(echo "$parsed" | sed '/^#/d' | sort -u)
aminusb "$all" "$listed"

echo
echo '##########'
echo '# wrong patterns'
echo '##########'
check_fx MAINTAINERS

# TODO: check overlaps
