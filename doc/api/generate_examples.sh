#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Luca Boccassi <bluca@debian.org>

EXAMPLES_DIR=$1
API_EXAMPLES=$2

exec > "${API_EXAMPLES}"
printf '/**\n'
printf '@page examples DPDK Example Programs\n\n'
find "${EXAMPLES_DIR}" -type f -name '*.c' -printf '@example examples/%P\n' | LC_ALL=C sort
printf '*/\n'
