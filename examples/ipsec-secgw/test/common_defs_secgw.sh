#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause

# check required parameters
SGW_REQ_VARS="SGW_PATH SGW_PORT_CFG SGW_WAIT_DEV"
for reqvar in ${SGW_REQ_VARS}
do
	if [[ -z "${!reqvar}" ]]; then
		echo "Required parameter ${reqvar} is empty"
		exit 127
	fi
done

# check if SGW_PATH point to an executable
if [[ ! -x ${SGW_PATH} ]]; then
	echo "${SGW_PATH} is not executable"
	exit 127
fi

# setup SGW_LCORE
SGW_LCORE=${SGW_LCORE:-0}

# setup config and output filenames
SGW_OUT_FILE=./ipsec-secgw.out1
SGW_CFG_FILE=$(mktemp)

# setup secgw parameters
SGW_CMD_EAL_PRM="--lcores=${SGW_LCORE} -n 4"
SGW_CMD_CFG="(0,0,${SGW_LCORE}),(1,0,${SGW_LCORE})"
SGW_CMD_PRM="-p 0x3 -u 1 -P --config=\"${SGW_CMD_CFG}\""

# start ipsec-secgw
secgw_start()
{
	SGW_EXEC_FILE=$(mktemp)
	cat <<EOF > ${SGW_EXEC_FILE}
stdbuf -o0 ${SGW_PATH} ${SGW_CMD_EAL_PRM} ${CRYPTO_DEV} \
${SGW_PORT_CFG} ${SGW_EAL_XPRM} \
-- ${SGW_CMD_PRM} ${SGW_CMD_XPRM} -f ${SGW_CFG_FILE} > \
${SGW_OUT_FILE} 2>&1 &
p=\$!
echo \$p
EOF

	cat ${SGW_EXEC_FILE}
	cat ${SGW_CFG_FILE}
	SGW_PID=`/bin/bash -x ${SGW_EXEC_FILE}`

	# wait till ipsec-secgw start properly
	i=0
	st=1
	while [[ $i -ne 10 && $st -ne 0 ]]; do
		sleep 1
		ifconfig ${SGW_WAIT_DEV}
		st=$?
		let i++
	done
}

# stop ipsec-secgw and cleanup
secgw_stop()
{
	kill ${SGW_PID}
	rm -f ${SGW_EXEC_FILE}
	rm -f ${SGW_CFG_FILE}
}
