#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

# check ETH_DEV
if [[ -z "${ETH_DEV}" ]]; then
	echo "ETH_DEV is invalid"
	exit 127
fi

# check that REMOTE_HOST is reachable
ssh ${REMOTE_HOST} echo
st=$?
if [[ $st -ne 0 ]]; then
	echo "host ${REMOTE_HOST} is not reachable"
	exit $st
fi

# get ether addr of REMOTE_HOST
REMOTE_MAC=`ssh ${REMOTE_HOST} ip addr show dev ${REMOTE_IFACE}`
st=$?
REMOTE_MAC=`echo ${REMOTE_MAC} | sed -e 's/^.*ether //' -e 's/ brd.*$//'`
if [[ $st -ne 0 || -z "${REMOTE_MAC}" ]]; then
	echo "couldn't retrieve ether addr from ${REMOTE_IFACE}"
	exit 127
fi

LOCAL_IFACE=dtap0

LOCAL_MAC="00:64:74:61:70:30"

REMOTE_IPV4=192.168.31.14
LOCAL_IPV4=192.168.31.92

REMOTE_IPV6=fd12:3456:789a:0031:0000:0000:0000:0014
LOCAL_IPV6=fd12:3456:789a:0031:0000:0000:0000:0092

DPDK_PATH=${PWD}
DPDK_BUILD="build"
DPDK_VARS=""

# by default ipsec-secgw can't deal with multi-segment packets
# make sure our local/remote host wouldn't generate fragmented packets
# if reassembly option is not enabled
DEF_MTU_LEN=1400
DEF_PING_LEN=1200

# set operation mode based on environment variables values
select_mode()
{
	echo "Test environment configuration:"
	# check which mode to be enabled (library/legacy)
	if [[ -n "${SGW_MODE}" && "${SGW_MODE}" == "library" ]]; then
		DPDK_MODE="-w 300 -l"
		echo "[enabled]  library mode"
	else
		DPDK_MODE=""
		echo "[enabled]  legacy mode"
	fi

	# check if esn is demanded
	if [[ -n "${SGW_ESN}" && "${SGW_ESN}" == "esn-on" ]]; then
		DPDK_VARS="${DPDK_VARS} -e"
		XFRM_ESN="flag esn"
		echo "[enabled]  extended sequence number"
	else
		XFRM_ESN=""
		echo "[disabled] extended sequence number"
	fi

	# check if atom is demanded
	if [[ -n "${SGW_ATOM}" && "${SGW_ATOM}" == "atom-on" ]]; then
		DPDK_VARS="${DPDK_VARS} -a"
		echo "[enabled]  sequence number atomic behavior"
	else
		echo "[disabled] sequence number atomic behavior"
	fi

	# check if inline should be enabled
	if [[ -n "${SGW_CRYPTO}" && "${SGW_CRYPTO}" == "inline" ]]; then
		CRYPTO_DEV='--vdev="crypto_null0"'
		SGW_CFG_XPRM_IN="port_id 0 type inline-crypto-offload"
		SGW_CFG_XPRM_OUT="port_id 0 type inline-crypto-offload"
		echo "[enabled]  inline crypto mode"
	else
		SGW_CFG_XPRM_IN=""
		SGW_CFG_XPRM_OUT=""
		echo "[disabled] inline crypto mode"
	fi

	# check if fallback should be enabled
	if [[ -n "${SGW_CRYPTO_FLBK}" ]] && [[ -n ${SGW_CFG_XPRM_IN} ]] \
	&& [[ "${SGW_MODE}" == "library" ]] \
	&& [[ "${SGW_CRYPTO_FLBK}" == "cpu-crypto" \
	|| "${SGW_CRYPTO_FLBK}" == "lookaside-none" ]]; then
		CRYPTO_DEV=""
		SGW_CFG_XPRM_IN="${SGW_CFG_XPRM_IN} fallback ${SGW_CRYPTO_FLBK}"
		SGW_CFG_XPRM_OUT=""
		echo "[enabled]  crypto fallback ${SGW_CRYPTO_FLBK} mode"
	else
		if [[ -n "${SGW_CRYPTO_FLBK}" \
		&& "${SGW_CRYPTO}" != "inline" ]]; then
			echo "SGW_CRYPTO variable needs to be set to \
\"inline\" for ${SGW_CRYPTO_FLBK} fallback setting"
			exit 127
		elif [[ -n "${SGW_CRYPTO_FLBK}" \
		&& "${SGW_MODE}" != "library" ]]; then
			echo "SGW_MODE variable needs to be set to \
\"library\" for ${SGW_CRYPTO_FLBK} fallback setting"
			exit 127
		fi
		echo "[disabled] crypto fallback mode"
	fi

	# select sync/async mode
	if [[ -n "${CRYPTO_PRIM_TYPE}" && -n "${DPDK_MODE}" ]]; then
		echo "[enabled]  crypto primary type - ${CRYPTO_PRIM_TYPE}"
		SGW_CFG_XPRM_IN="${SGW_CFG_XPRM_IN} type ${CRYPTO_PRIM_TYPE}"
		SGW_CFG_XPRM_OUT="${SGW_CFG_XPRM_OUT} type ${CRYPTO_PRIM_TYPE}"
	else
		if [[ -n "${CRYPTO_PRIM_TYPE}" \
		&& "${SGW_MODE}" != "library" ]]; then
			echo "SGW_MODE variable needs to be set to \
\"library\" for ${CRYPTO_PRIM_TYPE} crypto primary type setting"
			exit 127
		fi
	fi


	# make linux to generate fragmented packets
	if [[ -n "${SGW_MULTI_SEG}" && -n "${DPDK_MODE}" ]]; then
		echo -e "[enabled]  multi-segment test is enabled\n"
		SGW_CMD_XPRM="--reassemble ${SGW_MULTI_SEG}"
		PING_LEN=5000
		MTU_LEN=1500
	else
		if [[ -z "${SGW_MULTI_SEG}" \
		&& "${SGW_CFG_XPRM_IN}" == *fallback* ]]; then
			echo "SGW_MULTI_SEG environment variable needs \
to be set for ${SGW_CRYPTO_FLBK} fallback test"
			exit 127
		elif [[ -n "${SGW_MULTI_SEG}" \
		&& "${SGW_MODE}" != "library" ]]; then
			echo "SGW_MODE variable needs to be set to \
\"library\" for multiple segment reassemble setting"
		exit 127
		fi

		echo -e "[disabled] multi-segment test\n"
		PING_LEN=${DEF_PING_LEN}
		MTU_LEN=${DEF_MTU_LEN}
	fi
}

# setup mtu on local iface
set_local_mtu()
{
	mtu=$1
	ifconfig ${LOCAL_IFACE} mtu ${mtu}
	sysctl -w net.ipv6.conf.${LOCAL_IFACE}.mtu=${mtu}
}

# configure local host/ifaces
config_local_iface()
{
	ifconfig ${LOCAL_IFACE} ${LOCAL_IPV4}/24 up
	ifconfig ${LOCAL_IFACE}

	ip neigh flush dev ${LOCAL_IFACE}
	ip neigh add ${REMOTE_IPV4} dev ${LOCAL_IFACE} lladdr ${REMOTE_MAC}
	ip neigh show dev ${LOCAL_IFACE}
}

config6_local_iface()
{
	config_local_iface

	sysctl -w net.ipv6.conf.${LOCAL_IFACE}.disable_ipv6=0
	ip addr add  ${LOCAL_IPV6}/64 dev ${LOCAL_IFACE}

	ip -6 neigh add ${REMOTE_IPV6} dev ${LOCAL_IFACE} lladdr ${REMOTE_MAC}
	ip neigh show dev ${LOCAL_IFACE}
}

# configure remote host/iface
config_remote_iface()
{
	ssh ${REMOTE_HOST} ifconfig ${REMOTE_IFACE} down
	ssh ${REMOTE_HOST} ifconfig ${REMOTE_IFACE} ${REMOTE_IPV4}/24 up
	ssh ${REMOTE_HOST} ifconfig ${REMOTE_IFACE}

	ssh ${REMOTE_HOST} ip neigh flush dev ${REMOTE_IFACE}

	ssh ${REMOTE_HOST} ip neigh add ${LOCAL_IPV4} \
		dev ${REMOTE_IFACE} lladdr ${LOCAL_MAC}
	ssh ${REMOTE_HOST} ip neigh show dev ${REMOTE_IFACE}

	ssh ${REMOTE_HOST} iptables --flush
}

config6_remote_iface()
{
	config_remote_iface

	ssh ${REMOTE_HOST} sysctl -w \
		net.ipv6.conf.${REMOTE_IFACE}.disable_ipv6=0
	ssh ${REMOTE_HOST} ip addr add  ${REMOTE_IPV6}/64 dev ${REMOTE_IFACE}

	ssh ${REMOTE_HOST} ip -6 neigh add ${LOCAL_IPV6} \
		dev ${REMOTE_IFACE} lladdr ${LOCAL_MAC}
	ssh ${REMOTE_HOST} ip neigh show dev ${REMOTE_IFACE}

	ssh ${REMOTE_HOST} ip6tables --flush
}

# configure remote and local host/iface
config_iface()
{
	config_local_iface
	config_remote_iface
}

config6_iface()
{
	config6_local_iface
	config6_remote_iface
}

# secgw application parameters setup
SGW_PORT_CFG="--vdev=\"net_tap0,mac=fixed\" ${ETH_DEV}"
SGW_WAIT_DEV="${LOCAL_IFACE}"
. ${DIR}/common_defs_secgw.sh
