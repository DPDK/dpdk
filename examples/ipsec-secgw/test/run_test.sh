#! /bin/bash

# usage: /bin/bash run_test.sh [-46]
# Run all defined linux_test[4,6].sh test-cases one by one
# user has to setup properly the following environment variables:
#  SGW_PATH - path to the ipsec-secgw binary to test
#  REMOTE_HOST - ip/hostname of the DUT
#  REMOTE_IFACE - iface name for the test-port on DUT
#  ETH_DEV - ethernet device to be used on SUT by DPDK ('-w <pci-id>')
# Also user can optonally setup:
#  SGW_LCORE - lcore to run ipsec-secgw on (default value is 0)
#  CRYPTO_DEV - crypto device to be used ('-w <pci-id>')
#  if none specified appropriate vdevs will be created by the scrit
# refer to linux_test1.sh for more information

# All supported modes to test.
# naming convention:
# 'old' means that ipsec-secgw will run in legacy (non-librte_ipsec mode)
# 'tun/trs' refer to tunnel/transport mode respectively
LINUX_TEST="tun_aescbc_sha1 \
tun_aescbc_sha1_esn \
tun_aescbc_sha1_esn_atom \
tun_aesgcm \
tun_aesgcm_esn \
tun_aesgcm_esn_atom \
trs_aescbc_sha1 \
trs_aescbc_sha1_esn \
trs_aescbc_sha1_esn_atom \
trs_aesgcm \
trs_aesgcm_esn \
trs_aesgcm_esn_atom \
tun_aescbc_sha1_old \
tun_aesgcm_old \
trs_aescbc_sha1_old \
trs_aesgcm_old \
tun_aesctr_sha1 \
tun_aesctr_sha1_old \
tun_aesctr_sha1_esn \
tun_aesctr_sha1_esn_atom \
trs_aesctr_sha1 \
trs_aesctr_sha1_old \
trs_aesctr_sha1_esn \
trs_aesctr_sha1_esn_atom \
tun_3descbc_sha1 \
tun_3descbc_sha1_old \
tun_3descbc_sha1_esn \
tun_3descbc_sha1_esn_atom \
trs_3descbc_sha1 \
trs_3descbc_sha1_old \
trs_3descbc_sha1_esn \
trs_3descbc_sha1_esn_atom"

DIR=`dirname $0`

# get input options
st=0
run4=0
run6=0
while [[ ${st} -eq 0 ]]; do
	getopts ":46" opt
	st=$?
	if [[ "${opt}" == "4" ]]; then
		run4=1
	elif [[ "${opt}" == "6" ]]; then
		run6=1
	fi
done

if [[ ${run4} -eq 0 && ${run6} -eq 0 ]]; then
	exit 127
fi

for i in ${LINUX_TEST}; do

	echo "starting test ${i}"

	st4=0
	if [[ ${run4} -ne 0 ]]; then
		/bin/bash ${DIR}/linux_test4.sh ${i}
		st4=$?
		echo "test4 ${i} finished with status ${st4}"
	fi

	st6=0
	if [[ ${run6} -ne 0 ]]; then
		/bin/bash ${DIR}/linux_test6.sh ${i}
		st6=$?
		echo "test6 ${i} finished with status ${st6}"
	fi

	let "st = st4 + st6"
	if [[ $st -ne 0 ]]; then
		echo "ERROR test ${i} FAILED"
		exit $st
	fi
done
