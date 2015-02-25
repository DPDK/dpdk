#!/bin/sh
#
#   BSD LICENSE
#
#   Copyright 2014-2015 6WIND S.A.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of 6WIND S.A. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

temp=/tmp/${0##*/}.$$.c

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
	-c -o /dev/null "${temp}" 1>&${out} 2>&${err}
then
	rm -f "${temp}"
	printf "\
#ifndef %s
#define %s 1
#endif /* %s */

" "${macro}" "${macro}" "${macro}" >> "${file}" &&
	printf 'Defining %s.\n' "${macro}"
else
	rm -f "${temp}"
	printf "\
/* %s is not defined. */

" "${macro}" >> "${file}" &&
	printf 'Not defining %s.\n' "${macro}"
fi

exit
