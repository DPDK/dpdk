#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause

DIR=$(dirname $0)

if [ $(id -u) -ne 0 ]; then
	echo "Run as root"
	exit 1
fi

# check python requirements
python3 ${DIR}/pkttest.py check_reqs
if [ $? -ne 0 ]; then
	echo "Requirements for Python not met, exiting"
	exit 1
fi

# secgw application parameters setup
CRYPTO_DEV="--vdev=crypto_null0"
SGW_PORT_CFG="--vdev=net_tap0,mac=fixed --vdev=net_tap1,mac=fixed"
SGW_EAL_XPRM="--no-pci"
SGW_CMD_XPRM=-l
SGW_WAIT_DEV="dtap0"
. ${DIR}/common_defs_secgw.sh

echo "Running tests: $*"
for testcase in $*
do
	# check test file presence
	testfile="${DIR}/${testcase}.py"
	if [ ! -f ${testfile} ]; then
		echo "Invalid test ${testcase}"
		continue
	fi

	# prepare test config
	python3 ${testfile} config > ${SGW_CFG_FILE}
	if [ $? -ne 0 ]; then
		rm -f ${SGW_CFG_FILE}
		echo "Cannot get secgw configuration for test ${testcase}"
		exit 1
	fi

	# start the application
	secgw_start

	# setup interfaces
	ifconfig dtap0 up
	ifconfig dtap1 up

	# run the test
	echo "Running test case: ${testcase}"
	python3 ${testfile}
	st=$?

	# stop the application
	secgw_stop

	# report test result and exit on failure
	if [ $st -eq 0 ]; then
		echo "Test case ${testcase} succeeded"
	else
		echo "Test case ${testcase} failed!"
		exit $st
	fi
done
