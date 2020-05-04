#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

DIR=`dirname $0`

regular=0
inline_on=0
fallback_on=0
legacy_only=0
fallback_val="lookaside-none"
crypto_prim=""
multi_seg_val=""
while getopts ":iflsrc" opt
do
	case $opt in
		i)
			inline_on=1
			;;
		f)
			fallback_on=1
			;;
		l)
			legacy_only=1
			;;
		s)
			multi_seg_val="SGW_MULTI_SEG=128"
			;;
		r)
			regular=1
			;;
		c)
			crypto_prim="CRYPTO_PRIM_TYPE=cpu-crypto"
			fallback_val="cpu-crypto"
			;;
	esac
done
shift $((OPTIND -1))

PROTO=$1
MODE=$2

# test scenarios to set up for regular test
TEST_MODES_REGULAR="legacy \
library \
library_esn \
library_esn_atom"

# test scenarios to set up for inline test
TEST_MODES_INLINE="legacy_inline \
library_inline"

# test scenarios to set up for fallback test
TEST_MODES_FALLBACK="library_fallback"

# env variables to export for specific test scenarios
default="SGW_MODE=legacy SGW_ESN=esn-off SGW_ATOM=atom-off SGW_CRYPTO=regular \
SGW_CRYPTO_FLBK= ${multi_seg_val}"
legacy="${default} CRYPTO_PRIM_TYPE="
library="${default} SGW_MODE=library ${crypto_prim}"
library_esn="${default} SGW_MODE=library SGW_ESN=esn-on ${crypto_prim}"
library_esn_atom="${default} SGW_MODE=library SGW_ESN=esn-on SGW_ATOM=atom-on \
${crypto_prim}"
legacy_inline="${default} SGW_CRYPTO=inline CRYPTO_PRIM_TYPE="
library_inline="${default} SGW_MODE=library SGW_CRYPTO=inline CRYPTO_PRIM_TYPE="
library_fallback="${default} SGW_MODE=library SGW_CRYPTO=inline \
SGW_CRYPTO_FLBK=${fallback_val} SGW_MULTI_SEG=128 CRYPTO_PRIM_TYPE="

# export needed env variables and run tests
if [[ ${regular} -eq 1 ]]; then
	for i in ${TEST_MODES_REGULAR}; do
		if [[ ${legacy_only} -eq 1 && "${i}" != *legacy* ]]; then
			continue
		elif [[ ${legacy_only} -eq 0 && "${i}" == *legacy* ]]; then
			continue
		fi
		for x in ${!i}; do
			export ${x}
		done

		/bin/bash ${DIR}/linux_test.sh ${PROTO} ${MODE}
		st=$?
		if [[ ${st} -ne 0 ]]; then
			exit ${st}
		fi
	done
elif [[ ${inline_on} -eq 1 || ${fallback_on} -eq 1 ]]; then
	if [[ ${inline_on} -eq 1 ]]; then
		for i in ${TEST_MODES_INLINE}; do
			if [[ ${legacy_only} -eq 1 && "${i}" != *legacy* ]]
			then
				continue
			elif [[ ${legacy_only} -eq 0 && "${i}" == *legacy* ]]
			then
				continue
			fi
			for x in ${!i}; do
				export ${x}
			done

			/bin/bash ${DIR}/linux_test.sh ${PROTO} ${MODE}
			st=$?
			if [[ ${st} -ne 0 ]]; then
				exit ${st}
			fi
		done
	fi
	if [[ ${fallback_on} -eq 1 ]]; then
		for i in ${TEST_MODES_FALLBACK}; do
			for x in ${!i}; do
				export ${x}
			done

			/bin/bash ${DIR}/linux_test.sh ${PROTO} ${MODE}
			st=$?
			if [[ ${st} -ne 0 ]]; then
				exit ${st}
			fi
		done
	fi
fi
exit 0
