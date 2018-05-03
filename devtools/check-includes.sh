#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 6WIND S.A.

# This script checks that header files in a given directory do not miss
# dependencies when included on their own, do not conflict and accept being
# compiled with the strictest possible flags.
#
# Files are looked up in the directory provided as the first argument,
# otherwise build/include by default.
#
# Recognized environment variables:
#
# VERBOSE=1 is the same as -v.
#
# QUIET=1 is the same as -q.
#
# SUMMARY=1 is the same as -s.
#
# CC, CPPFLAGS, CFLAGS, EXTRA_CPPFLAGS, EXTRA_CFLAGS, CXX, CXXFLAGS and
# EXTRA_CXXFLAGS are taken into account.
#
# PEDANTIC_CFLAGS, PEDANTIC_CXXFLAGS and PEDANTIC_CPPFLAGS provide strict
# C/C++ compilation flags.
#
# IGNORE contains a list of shell patterns matching files (relative to the
# include directory) to avoid. It is set by default to known DPDK headers
# which must not be included on their own.
#
# IGNORE_CXX provides additional files for C++.

while getopts hqvs arg; do
	case $arg in
	h)
		cat <<EOF
usage: $0 [-h] [-q] [-v] [-s] [DIR]

This script checks that header files in a given directory do not miss
dependencies when included on their own, do not conflict and accept being
compiled with the strictest possible flags.

  -h    display this help and exit
  -q    quiet mode, disable normal output
  -v    show command lines being executed
  -s    show summary

With no DIR, default to build/include.

Any failed header check yields a nonzero exit status.
EOF
		exit
		;;
	q)
		QUIET=1
		;;
	v)
		VERBOSE=1
		;;
	s)
		SUMMARY=1
		;;
	*)
		exit 1
		;;
	esac
done

shift $(($OPTIND - 1))

include_dir=${1:-build/include}

: ${PEDANTIC_CFLAGS=-std=c99 -pedantic -Wall -Wextra -Werror}
: ${PEDANTIC_CXXFLAGS=}
: ${PEDANTIC_CPPFLAGS=-D_XOPEN_SOURCE=600}
: ${CC:=cc}
: ${CXX:=c++}
: ${IGNORE= \
	'rte_atomic_32.h' \
	'rte_atomic_64.h' \
	'rte_byteorder_32.h' \
	'rte_byteorder_64.h' \
	'generic/*' \
	'exec-env/*' \
	'rte_vhost.h' \
	'rte_eth_vhost.h' \
	'rte_eal_interrupts.h' \
}
: ${IGNORE_CXX= \
	'rte_vhost.h' \
	'rte_eth_vhost.h' \
}

temp_cc=/tmp/${0##*/}.$$.c
pass_cc=
failures_cc=0

temp_cxx=/tmp/${0##*/}.$$.cc
pass_cxx=
failures_cxx=0

# Process output parameters.

[ "$QUIET" = 1 ] &&
exec 1> /dev/null

[ "$VERBOSE" = 1 ] &&
output ()
{
	local CCV
	local CXXV

	shift
	CCV=$CC
	CXXV=$CXX
	CC="echo $CC" CXX="echo $CXX" "$@"
	CC=$CCV
	CXX=$CXXV

	"$@"
} ||
output ()
{

	printf '  %s\n' "$1"
	shift
	"$@"
}

trap 'rm -f "$temp_cc" "$temp_cxx"' EXIT

compile_cc ()
{
	${CC} -I"$include_dir" \
		${PEDANTIC_CPPFLAGS} ${CPPFLAGS} ${EXTRA_CPPFLAGS} \
		${PEDANTIC_CFLAGS} ${CFLAGS} ${EXTRA_CFLAGS} \
		-c -o /dev/null "${temp_cc}"
}

compile_cxx ()
{
	${CXX} -I"$include_dir" \
		${PEDANTIC_CPPFLAGS} ${CPPFLAGS} ${EXTRA_CPPFLAGS} \
		${PEDANTIC_CXXFLAGS} ${CXXFLAGS} ${EXTRA_CXXFLAGS} \
		-c -o /dev/null "${temp_cxx}"
}

ignore ()
{
	file="$1"
	shift
	while [ $# -ne 0 ]; do
		case "$file" in
		$1)
			return 0
			;;
		esac
		shift
	done
	return 1
}

# Check C/C++ compilation for each header file.

while read -r path
do
	file=${path#$include_dir}
	file=${file##/}
	if ignore "$file" $IGNORE; then
		output "SKIP $file" :
		continue
	fi
	if printf "\
#include <%s>

int main(void)
{
	return 0;
}
" "$file" > "$temp_cc" &&
		output "CC $file" compile_cc
	then
		pass_cc="$pass_cc $file"
	else
		failures_cc=$(($failures_cc + 1))
	fi
	if ignore "$file" $IGNORE_CXX; then
		output "SKIP CXX $file" :
		continue
	fi
	if printf "\
#include <%s>

int main()
{
}
" "$file" > "$temp_cxx" &&
		output "CXX $file" compile_cxx
	then
		pass_cxx="$pass_cxx $file"
	else
		failures_cxx=$(($failures_cxx + 1))
	fi
done <<EOF
$(find "$include_dir" -name '*.h')
EOF

# Check C compilation with all includes.

: > "$temp_cc" &&
for file in $pass_cc; do
	printf "\
#include <%s>
" "$file" >> $temp_cc
done
if printf "\
int main(void)
{
	return 0;
}
" >> "$temp_cc" &&
	output "CC (all includes that did not fail)" compile_cc
then
	:
else
	failures_cc=$(($failures_cc + 1))
fi

# Check C++ compilation with all includes.

: > "$temp_cxx" &&
for file in $pass_cxx; do
	printf "\
#include <%s>
" "$file" >> $temp_cxx
done
if printf "\
int main()
{
}
" >> "$temp_cxx" &&
	output "CXX (all includes that did not fail)" compile_cxx
then
	:
else
	failures_cxx=$(($failures_cxx + 1))
fi

# Report results.

if [ "$SUMMARY" = 1 ]; then
	printf "\
Summary:
 %u failure(s) for C using '%s'.
 %u failure(s) for C++ using '%s'.
" $failures_cc "$CC" $failures_cxx "$CXX" 1>&2
fi

# Exit with nonzero status if there are failures.

[ $failures_cc -eq 0 ] &&
[ $failures_cxx -eq 0 ]
