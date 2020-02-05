#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

arfile=$1
output=$2
pmdinfogen=$3

# The generated file must not be empty if compiled in pedantic mode
echo 'static __attribute__((unused)) const char *generator = "'$0'";' > $output
for ofile in `ar t $arfile` ; do
	ar p $arfile $ofile | $pmdinfogen - - >> $output 2> /dev/null
done
exit 0
