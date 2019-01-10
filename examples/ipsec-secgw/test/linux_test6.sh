#! /bin/bash

# usage:  /bin/bash linux_test6.sh <ipsec_mode>
# for list of available modes please refer to run_test.sh.
# ipsec-secgw (IPv6 mode) functional test script.
#
# Note that for most of them you required appropriate crypto PMD/device
# to be avaialble.
# Also user has to setup properly the following environment variables:
#  SGW_PATH - path to the ipsec-secgw binary to test
#  REMOTE_HOST - ip/hostname of the DUT
#  REMOTE_IFACE - iface name for the test-port on DUT
#  ETH_DEV - ethernet device to be used on SUT by DPDK ('-w <pci-id>')
# Also user can optonally setup:
#  SGW_LCORE - lcore to run ipsec-secgw on (default value is 0)
#  CRYPTO_DEV - crypto device to be used ('-w <pci-id>')
#  if none specified appropriate vdevs will be created by the scrit
#
# The purpose of the script is to automate ipsec-secgw testing
# using another system running linux as a DUT.
# It expects that SUT and DUT are connected through at least 2 NICs.
# One NIC is expected to be managed by linux both machines,
# and will be used as a control path.
# Make sure user from SUT can ssh to DUT without entering password,
# also make sure that sshd over ipv6 is enabled.
# Second NIC (test-port) should be reserved for DPDK on SUT,
# and should be managed by linux on DUT.
# The script starts ipsec-secgw with 2 NIC devices: test-port and tap vdev.
# Then configures local tap iface and remote iface and ipsec policies
# in the following way:
# traffic going over test-port in both directions has to be
# protected by ipsec.
# raffic going over TAP in both directions doesn't have to be protected.
# I.E:
# DUT OS(NIC1)--(ipsec)-->(NIC1)ipsec-secgw(TAP)--(plain)-->(TAP)SUT OS
# SUT OS(TAP)--(plain)-->(TAP)psec-secgw(NIC1)--(ipsec)-->(NIC1)DUT OS
# Then tries to perorm some data transfer using the scheme decribed above.
#

DIR=`dirname $0`
MODE=$1

 . ${DIR}/common_defs.sh
 . ${DIR}/${MODE}_defs.sh

config_secgw

secgw_start

config6_iface

config6_remote_xfrm

 . ${DIR}/data_rxtx.sh

ping6_test1 ${REMOTE_IPV6}
st=$?
if [[ $st -eq 0 ]]; then
	scp_test1 ${REMOTE_IPV6}
	st=$?
fi

secgw_stop
exit $st
