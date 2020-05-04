#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

# Usage: /bin/bash linux_test.sh <ip_protocol> <ipsec_mode>
# <ip_protocol> can be set to:
#  ipv4-ipv4 - only IPv4 traffic
#  ipv4-ipv6 - IPv4 traffic over IPv6 ipsec tunnel (only for tunnel mode)
#  ipv6-ipv4 - IPv6 traffic over IPv4 ipsec tunnel (only for tunnel mode)
#  ipv6-ipv6 - only IPv6 traffic
# For list of available modes please refer to run_test.sh.
#
# Note that most of them require appropriate crypto PMD/device to be available.
# Also user has to setup properly the following environment variables:
#  SGW_PATH	- path to the ipsec-secgw binary to test
#  REMOTE_HOST	- ip/hostname of the DUT
#  REMOTE_IFACE	- iface name for the test-port on DUT
#  ETH_DEV	- ethernet device to be used on SUT by DPDK ('-w <pci-id>')
# Also user can optionally setup:
#  SGW_LCORE	- lcore to run ipsec-secgw on (default value is 0)
#  SGW_MODE	- run ipsec-secgw in legacy mode or with use of library
#		values: legacy/library (legacy on default)
#  SGW_ESN	- run ipsec-secgw with extended sequence number
#		values: esn-on/esn-off (esn-off on default)
#  SGW_ATOM	- run ipsec-secgw with sequence number atomic behavior
#		values: atom-on/atom-off (atom-off on default)
#  SGW_CRYPTO	- run ipsec-secgw with use of inline crypto
#		values: inline (unset on default)
#  SGW_CRYPTO_FLBK - run ipsec-secgw with crypto fallback configured
#		values: cpu-crypto/lookaside-none (unset on default)
#  CRYPTO_PRIM_TYPE - run ipsec-secgw with crypto primary type set
#		values: cpu-crypto (unset on default)
#  CRYPTO_DEV - crypto device to be used ('-w <pci-id>')
#	       if none specified appropriate vdevs will be created by the script
#  SGW_MULTI_SEG - ipsec-secgw option to enable reassembly support and
#		specify size of reassembly table (i.e. SGW_MULTI_SEG=128)
#
# The purpose of the script is to automate ipsec-secgw testing
# using another system running linux as a DUT.
# It expects that SUT and DUT are connected through at least 2 NICs.
# One NIC is expected to be managed by linux both machines,
# and will be used as a control path
# Make sure user from SUT can ssh to DUT without entering password.
# Second NIC (test-port) should be reserved for DPDK on SUT,
# and should be managed by linux on DUT.
# The script starts ipsec-secgw with 2 NIC devices: test-port and tap vdev.
# Then configures local tap iface and remote iface and ipsec policies
# in the following way:
# traffic going over test-port in both directions has to be
# protected by ipsec.
# Traffic going over TAP in both directions doesn't have to be protected.
# I.E:
# DUT OS(NIC1)--(ipsec)-->(NIC1)ipsec-secgw(TAP)--(plain)-->(TAP)SUT OS
# SUT OS(TAP)--(plain)-->(TAP)psec-secgw(NIC1)--(ipsec)-->(NIC1)DUT OS
# Then tries to perform some data transfer using the scheme described above.
#

DIR=`dirname $0`
PROTO=$1
MODE=$2

 . ${DIR}/common_defs.sh

select_mode

 . ${DIR}/${MODE}_defs.sh

if [[ "${PROTO}" == "ipv4-ipv4" ]] || [[ "${PROTO}" == "ipv6-ipv6" ]]; then
	config_secgw
else
	config_secgw_mixed
fi

secgw_start

 . ${DIR}/data_rxtx.sh

if [[ "${PROTO}" == "ipv4-ipv4" ]]; then
	config_iface
	config_remote_xfrm_44
	set_local_mtu ${MTU_LEN}
	ping_test1 ${REMOTE_IPV4} 0 ${PING_LEN}

	st=$?
	if [[ $st -eq 0 ]]; then
		set_local_mtu ${DEF_MTU_LEN}
		scp_test1 ${REMOTE_IPV4}
		st=$?
	fi
elif [[ "${PROTO}" == "ipv4-ipv6" ]]; then
	if [[ "${MODE}" == trs* ]]; then
		echo "Cannot mix protocols in transport mode"
		secgw_stop
		exit 1
	fi
	config6_iface
	config_remote_xfrm_46
	set_local_mtu ${MTU_LEN}
	ping_test1 ${REMOTE_IPV4} 0 ${PING_LEN}

	st=$?
	if [[ $st -eq 0 ]]; then
		set_local_mtu ${DEF_MTU_LEN}
		scp_test1 ${REMOTE_IPV4}
		st=$?
	fi
elif [[ "${PROTO}" == "ipv6-ipv4" ]]; then
	if [[ "${MODE}" == trs* ]]; then
		echo "Cannot mix protocols in transport mode"
		secgw_stop
		exit 1
	fi
	config6_iface
	config_remote_xfrm_64

	set_local_mtu ${MTU_LEN}
	ping6_test1 ${REMOTE_IPV6} 0 ${PING_LEN}
	st=$?
	if [[ $st -eq 0 ]]; then
		set_local_mtu ${DEF_MTU_LEN}
		scp_test1 ${REMOTE_IPV6}
		st=$?
	fi
elif [[ "${PROTO}" == "ipv6-ipv6" ]]; then
	config6_iface
	config_remote_xfrm_66
	set_local_mtu ${MTU_LEN}
	ping6_test1 ${REMOTE_IPV6} 0 ${PING_LEN}

	st=$?
	if [[ $st -eq 0 ]]; then
		set_local_mtu ${DEF_MTU_LEN}
		scp_test1 ${REMOTE_IPV6}
		st=$?
	fi
else
	echo "Invalid <proto>"
	st=128
fi

secgw_stop
exit $st
