#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 David Marchand <david.marchand@redhat.com>

section=all
symbol=all
quiet=
version=

while getopts 'S:s:qV:' name; do
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
	V)
		version=$OPTARG
	;;
	?)
		echo 'usage: $0 [-S section] [-s symbol] [-V [version|unset]] [-q]'
		exit 1
	;;
	esac
done

shift $(($OPTIND - 1))

for file in $@; do
	cat "$file" |awk '
	BEGIN {
		current_section = "";
		if ("'$section'" == "all" && "'$symbol'" == "all" && "'$version'" == "") {
			ret = 0;
		} else {
			ret = 1;
		}
	}
	/^.*\{/ {
		if ("'$section'" == "all" || $1 == "'$section'") {
			current_section = $1;
		}
	}
	/.*}/ { current_section = ""; }
	/^[^}].*[^:*];/ {
		if (current_section == "") {
			next;
		}
		if (/^[^}].*[^:*]; # added in /) {
			symbol_version = $5
		}
		if ("'$version'" != "") {
			if ("'$version'" == "unset" && symbol_version != "") {
				next;
			} else if ("'$version'" != "unset" && "'$version'" != symbol_version) {
				next;
			}
		}
		gsub(";","");
		if ("'$symbol'" == "all" || $1 == "'$symbol'") {
			ret = 0;
			if ("'$quiet'" == "") {
				print "'$file' "current_section" "$1" "symbol_version;
			}
			if ("'$symbol'" != "all") {
				exit 0;
			}
		}
	}
	END {
		exit ret;
	}'
done
