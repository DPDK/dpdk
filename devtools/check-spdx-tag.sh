#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Microsoft Corporation
#
# Produce a list of files with incorrect license tags

missing_spdx=0
wrong_license=0
warnings=0
quiet=false
verbose=false

print_usage () {
    echo "usage: $(basename $0) [-q] [-v]"
    exit 1
}

no_license_list=\
':^.git* :^.mailmap :^.ci/* :^README :^*/README* :^MAINTAINERS :^VERSION :^ABI_VERSION '\
':^license/ :^config/ :^buildtools/ :^*.abignore :^*.cocci :^*/poetry.lock '\
':^*/Kbuild :^kernel/linux/uapi/version '\
':^*.ini :^*.data :^*.json :^*.cfg :^*.txt :^*.svg :^*.png'

check_spdx() {
    if $verbose ; then
	echo "Files without SPDX License"
	echo "--------------------------"
    fi
    git grep -L SPDX-License-Identifier -- $no_license_list > $tmpfile

    missing_spdx=$(wc -l < $tmpfile)
    $quiet || cat $tmpfile
}

build_exceptions_list() {
    grep '.*|.*|.*|.*' license/exceptions.txt | grep -v 'TB Approval Date' |
    while IFS='|' read license tb_date gb_date pattern ; do
        license=$(echo $license) # trim spaces
        git grep -l "SPDX-License-Identifier:[[:space:]]*$license" $pattern |
        sed -e 's/^/:^/'
    done
}

check_licenses() {
    if $verbose ; then
	echo "Files with wrong license and no exception"
	echo "-----------------------------------------"
    fi
    exceptions=$(build_exceptions_list)
    git grep -l SPDX-License-Identifier: -- $no_license_list $exceptions |
    xargs grep -L -E 'SPDX-License-Identifier:[[:space:]]*(\(?|.* OR )BSD-3-Clause' > $tmpfile

    wrong_license=$(wc -l < $tmpfile)
    $quiet || cat $tmpfile
}

check_boilerplate() {
    if $verbose ; then
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
$verbose && echo

check_licenses
$verbose && echo

check_boilerplate
$verbose && echo

echo "total: $missing_spdx missing SPDX, $wrong_license license errors, $warnings warnings"
exit $((missing_spdx + wrong_license))
