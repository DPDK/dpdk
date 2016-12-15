#! /bin/sh

# BSD LICENSE
#
# Copyright 2015 6WIND S.A.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of 6WIND S.A. nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Do some basic checks in MAINTAINERS file

cd $(dirname $0)/..

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
			whitelist=$(files $flines)
			blacklist=$(files $xlines)
			match=$(aminusb "$whitelist" "$blacklist")
			if [ -n "$whitelist" ] ; then
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
