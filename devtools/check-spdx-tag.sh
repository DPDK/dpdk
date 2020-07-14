#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Microsoft Corporation
#
# Produce a list of files with incorrect license tags

errors=0
warnings=0
quiet=false
verbose=false

print_usage () {
    echo "usage: $(basename $0) [-q] [-v]"
    exit 1
}

check_spdx() {
    if  $verbose;  then
	echo "Files without SPDX License"
	echo "--------------------------"
    fi
    git grep -L SPDX-License-Identifier -- \
	':^.git*' ':^.ci/*' ':^.travis.yml' \
	':^README' ':^MAINTAINERS' ':^VERSION' ':^ABI_VERSION' \
	':^*/Kbuild' ':^*/README' \
	':^license/' ':^config/' ':^buildtools/' \
	':^*.cocci' ':^*.abignore' \
	':^*.def' ':^*.map' ':^*.ini' ':^*.data' ':^*.cfg' ':^*.txt' \
	':^*.svg' ':^*.png'\
	> $tmpfile

    errors=$(wc -l < $tmpfile)
    $quiet || cat $tmpfile
}

check_boilerplate() {
    if $verbose ; then
	echo
	echo "Files with redundant license text"
	echo "---------------------------------"
    fi

    git grep -l Redistribution -- \
	':^license/' ':^/devtools/check-spdx-tag.sh' > $tmpfile

    warnings=$(wc -l <$tmpfile)
    $quiet || cat $tmpfile
}

while getopts qvh ARG ; do
	case $ARG in
		q ) quiet=true ;;
		v ) verbose=true ;;
		h ) print_usage ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))

tmpfile=$(mktemp -t dpdk.checkspdx.XXXXXX)
trap 'rm -f -- "$tmpfile"' INT TERM HUP EXIT

check_spdx
$quiet || echo

check_boilerplate

$quiet || echo
echo "total: $errors errors, $warnings warnings"
exit $errors
