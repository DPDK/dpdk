#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Luca Boccassi <bluca@debian.org>

DOXYCONF=$1
OUTDIR=$2
SCRIPTCSS=$3

# run doxygen, capturing all the header files it processed
doxygen "${DOXYCONF}" | tee doxygen.out
echo "$OUTDIR: $(awk '/Preprocessing/ {printf("%s ", substr($2, 1, length($2) - 3))}' doxygen.out)" > $OUTDIR.d

"${SCRIPTCSS}" "${OUTDIR}"/doxygen.css
