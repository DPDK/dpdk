#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 David Marchand <david.marchand@redhat.com>

section=all
symbol=all
quiet=

while getopts 'S:s:q' name; do
	case $name in
	S)
		[ $section = 'all' ] || {
			echo 'Cannot list in multiple sections'
			exit 1
		}
		section=$OPTARG
	;;
	s)
		[ $symbol = 'all' ] || {
			echo 'Cannot list multiple symbols'
			exit 1
		}
		symbol=$OPTARG
	;;
	q)
		quiet='y'
	;;
	?)
		echo 'usage: $0 [-S section] [-s symbol] [-q]'
		exit 1
	;;
	esac
done

shift $(($OPTIND - 1))

for file in $@; do
	cat "$file" |awk '
	BEGIN {
		current_section = "";
		if ("'$section'" == "all" && "'$symbol'" == "all") {
			ret = 0;
		} else {
			ret = 1;
		}
	}
	/^.*{/ {
		if ("'$section'" == "all" || $1 == "'$section'") {
			current_section = $1;
		}
	}
	/.*}/ { current_section = ""; }
	/^[^}].*[^:*];/ {
		if (current_section != "") {
			gsub(";","");
			if ("'$symbol'" == "all" || $1 == "'$symbol'") {
				ret = 0;
				if ("'$quiet'" == "") {
					print "'$file' "current_section" "$1;
				}
				if ("'$symbol'" != "all") {
					exit 0;
				}
			}
		}
	}
	END {
		exit ret;
	}'
done
