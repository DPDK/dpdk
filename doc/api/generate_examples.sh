#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Luca Boccassi <bluca@debian.org>

EXAMPLES_DIR=$1
API_EXAMPLES=$2

FIND=find
if [ "$(uname)" = "FreeBSD" ] ; then
# on FreeBSD, we need GNU find for "-printf" flag
	FIND=gfind
	if ! which -s $FIND ; then
		echo "Error: need '$FIND' on FreeBSD. Install 'findutils' pkg"
		exit 1
	fi
fi

# generate a .d file including both C files and also build files, so we can
# detect both file changes and file additions/deletions
echo "$API_EXAMPLES: $($FIND ${EXAMPLES_DIR} -type f \( -name '*.c' -o -name 'meson.build' \) -printf '%p ' )" > ${API_EXAMPLES}.d

exec > "${API_EXAMPLES}"
printf '/**\n'
printf '@page examples DPDK Example Programs\n\n'
$FIND "${EXAMPLES_DIR}" -type f -name '*.c' -printf '@example examples/%P\n' | LC_ALL=C sort
printf '*/\n'
