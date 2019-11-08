#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

CRYPTO_DEV=${CRYPTO_DEV:-'--vdev="crypto_null0"'}

#generate cfg file for ipsec-secgw
config_secgw()
{
	cat <<EOF > ${SGW_CFG_FILE}

sp ipv4 in esp bypass pri 1 sport 0:65535 dport 0:65535
sp ipv6 in esp bypass pri 1 sport 0:65535 dport 0:65535

sp ipv4 out esp bypass pri 1 sport 0:65535 dport 0:65535
sp ipv6 out esp bypass pri 1 sport 0:65535 dport 0:65535

#Routing rules
rt ipv4 dst ${REMOTE_IPV4}/32 port 0
rt ipv4 dst ${LOCAL_IPV4}/32 port 1

rt ipv6 dst ${REMOTE_IPV6}/128 port 0
rt ipv6 dst ${LOCAL_IPV6}/128 port 1

#neighbours
neigh port 0 ${REMOTE_MAC}
neigh port 1 ${LOCAL_MAC}
EOF

	cat ${SGW_CFG_FILE}
}

SGW_CMD_XPRM='-w 300 -l'

config_remote_xfrm()
{
	ssh ${REMOTE_HOST} ip xfrm policy flush
	ssh ${REMOTE_HOST} ip xfrm state flush

	ssh ${REMOTE_HOST} ip xfrm policy list
	ssh ${REMOTE_HOST} ip xfrm state list
}

config6_remote_xfrm()
{
	config_remote_xfrm
}
