#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

# Usage:
# /bin/bash <test-acl-binary> <dir-with-acl-rules-traces> <acl-alg> <burst-size>
# Expected file-naming conventions:
#   - for rules: 'acl[0-9]v[4,6]_[0-9,a-z]+_rule'
#   - for traces: 'acl[0-9]v[4,6]_[0-9,a-z]+_trace'
# Each rule file expects to have exactly one trace file.
# test-acl app follows classbench file format.
# Each line defines exactly one rule/trace.
# rules record format:
# '@'<src_ip_addr>'/'<masklen><space> \
# <dst_ipv4_addr>'/'<masklen><space> \
# <src_port_low><space>":"<src_port_high><space> \
# <dst_port_low><space>":"<dst_port_high><space> \
# <proto>'/'<mask>
# trace record format:
# <src_ip_addr><space><dst_ip_addr><space> \
# <src_port><space<dst_port><space><proto>...<rule_id>
#
# As an example:
# /bin/bash app/test-acl/test-acl.sh build/app/dpdk-test-acl \
# app/test-acl/input scalar 32
#
# Refer to test-acl app for more information about rules/trace files format,
# available test-acl command-line options, etc.

TACL_PATH=$1
TACL_DIR=$2
TACL_ALG=$3
TACL_STEP=$4

if [[ ! -x ${TACL_PATH} ]]; then
	echo "invalid TACL_PATH=${TACL_PATH}"
	exit 127
fi

if [[ ! -d ${TACL_DIR} ]]; then
	echo "invalid TACL_DIR=${TACL_DIR}"
	exit 127
fi

V4F=`find ${TACL_DIR} -type f | egrep -e 'acl[0-9]v4_[0-9,a-z]+_rule$'`
V6F=`find ${TACL_DIR} -type f | egrep -e 'acl[0-9]v6_[0-9,a-z]+_rule$'`

run_test()
{
	i=$1
	n=`basename ${i}`

	TRACEF=`echo ${i} | sed -e 's/_rule$/_trace/'`
	if [[ ! -f ${TRACEF} ]]; then
		echo "${TRACEF} not found"
		echo "test ${n} FAILED"
		exit 127
	fi

	OUTF=`mktemp ${n}_XXXXXX`
	echo "start test ${n} with alg ${TACL_ALG}, burst-size ${TACL_STEP}"
	${TACL_PATH} -l 0 -n 4 --log-level="acl,debug" \
		--force-max-simd-bitwidth=0 --no-pci -- \
		${XPRM} --tracenum=200000 --rulesf=${i} --tracef=${TRACEF} \
		--tracestep=${TACL_STEP} --alg=${TACL_ALG} \
		> ${OUTF}
	grep 'result:' ${OUTF} | awk '{print $(NF);}' > ${OUTF}.out
	sed -e '/^[[:space:]]*#/d' \
		-e '/^[[:space:]]*$/d' \
		-e 's/[[:space:]]*$//g' ${TRACEF} | \
		awk '{print $(NF);}' > ${OUTF}.chk
	diff -u ${OUTF}.chk ${OUTF}.out
	st=$?
	if [[ $st -ne 0 ]]; then
	echo "test ${n} FAILED"
	echo "output files:"
		ls ${OUTF}*
		cat ${OUTF}*
		exit 127
	fi
	rm -f ${OUTF}*
	echo "test ${n} OK"
}

for i in ${V4F}; do
	run_test $i
done

for i in ${V6F}; do
	XPRM='--ipv6'
	run_test $i
	unset XPRM
done

echo "All tests have ended successfully"
