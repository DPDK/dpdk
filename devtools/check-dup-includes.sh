#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 Mellanox Technologies, Ltd

# Check C files in git repository for duplicated includes.
# Usage: devtools/check-dup-includes.sh [directory]

dir=${1:-$(dirname $(readlink -f $0))/..}
cd $dir

# speed up by ignoring Unicode details
export LC_ALL=C

for file in $(git ls-files '*.[ch]') ; do
	sed -rn 's,^[[:space:]]*#include[[:space:]]*[<"](.*)[>"].*,\1,p' $file |
	sort | uniq -d |
	sed "s,^,$file: duplicated include: ,"
done
