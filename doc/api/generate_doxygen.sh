#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Luca Boccassi <bluca@debian.org>

DOXYCONF=$1
OUTDIR=$2
SCRIPTCSS=$3

doxygen "${DOXYCONF}"
"${SCRIPTCSS}" "${OUTDIR}"/doxygen.css
