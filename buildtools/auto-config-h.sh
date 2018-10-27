#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2015 6WIND S.A.
#
# Crude script to detect whether particular types, macros and functions are
# defined by trying to compile a file with a given header. Can be used to
# perform cross-platform checks since the resulting object file is not
# executed.
#
# Set VERBOSE=1 in the environment to display compiler output and errors.
#
# CC, CPPFLAGS, CFLAGS, EXTRA_CPPFLAGS and EXTRA_CFLAGS are taken from the
# environment.
#
# AUTO_CONFIG_CFLAGS may append additional CFLAGS without modifying the
# above variables.

file=${1:?output file name required (config.h)}
macro=${2:?output macro name required (HAVE_*)}
include=${3:?include name required (foo.h)}
type=${4:?object type required (define, enum, type, field, func)}
name=${5:?define/type/function name required}

: ${CC:=cc}

temp=$(mktemp -t dpdk.${0##*/}.c.XXXXXX)

case $type in
define)
	code="\
#ifndef $name
#error $name not defined
#endif
"
	;;
enum)
	code="\
long test____ = $name;
"
	;;
type)
	code="\
$name test____;
"
	;;
field)
	code="\
void test____(void)
{
	${name%%.*} test_____;

	(void)test_____.${name#*.};
}
"
	;;
func)
	code="\
void (*test____)() = (void (*)())$name;
"
	;;
*)
	unset error
	: ${error:?unknown object type \"$type\"}
	exit
esac

if [ "${VERBOSE}" = 1 ]
then
	err=2
	out=1
	eol='
'
else
	exec 3> /dev/null ||
	exit
	err=3
	out=3
	eol=' '
fi &&
printf 'Looking for %s %s in %s.%s' \
	"${name}" "${type}" "${include}" "${eol}" &&
printf "\
#include <%s>

%s
" "$include" "$code" > "${temp}" &&
if ${CC} ${CPPFLAGS} ${EXTRA_CPPFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} \
	${AUTO_CONFIG_CFLAGS} \
	-xc -c -o ${temp}.o "${temp}" 1>&${out} 2>&${err}
then
	rm -f "${temp}" "${temp}.o"
	printf "\
#ifndef %s
#define %s 1
#endif /* %s */

" "${macro}" "${macro}" "${macro}" >> "${file}" &&
	printf 'Defining %s.\n' "${macro}"
else
	rm -f "${temp}" "${temp}.o"
	printf "\
/* %s is not defined. */

" "${macro}" >> "${file}" &&
	printf 'Not defining %s.\n' "${macro}"
fi

exit
