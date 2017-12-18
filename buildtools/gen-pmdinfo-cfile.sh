#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

arfile=$1
output=$2
pmdinfogen=$3

tmp_o=${output%.c.pmd.c}.tmp.o

ar p $arfile > $tmp_o && \
		$pmdinfogen $tmp_o $output
