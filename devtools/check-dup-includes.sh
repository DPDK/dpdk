#! /bin/sh -e

# Copyright 2017 Mellanox Technologies, Ltd. All rights reserved.
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
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  SOFTWARE.

# Check C files in git repository for duplicated includes.
# Usage: devtools/check-dup-includes.sh [directory]

dir=${1:-$(dirname $(readlink -m $0))/..}
cd $dir

# speed up by ignoring Unicode details
export LC_ALL=C

for file in $(git ls-files '*.[ch]') ; do
	sed -rn 's,^[[:space:]]*#include[[:space:]]*[<"](.*)[>"].*,\1,p' $file |
	sort | uniq -d |
	sed "s,^,$file: duplicated include: ,"
done
