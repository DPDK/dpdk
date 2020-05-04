#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

CRYPTO_DEV=${CRYPTO_DEV:-'--vdev="crypto_aesni_mb0"'}

#generate cfg file for ipsec-secgw
config_secgw()
{
	cat <<EOF > ${SGW_CFG_FILE}
#SP in IPv4 rules
sp ipv4 in esp protect 7 pri 2 src ${REMOTE_IPV4}/32 dst ${LOCAL_IPV4}/32 \
sport 0:65535 dport 0:65535
sp ipv4 in esp bypass pri 1 sport 0:65535 dport 0:65535

#SP out IPv4 rules
sp ipv4 out esp protect 7 pri 2 src ${LOCAL_IPV4}/32 dst ${REMOTE_IPV4}/32 \
sport 0:65535 dport 0:65535
sp ipv4 out esp bypass pri 1 sport 0:65535 dport 0:65535

#sp in IPv6 rules
sp ipv6 in esp protect 9 pri 2 src ${REMOTE_IPV6}/128 dst ${LOCAL_IPV6}/128 \
sport 0:65535 dport 0:65535
sp ipv6 in esp bypass pri 1 sport 0:65535 dport 0:65535

#SP out IPv6 rules
sp ipv6 out esp protect 9 pri 2 src ${LOCAL_IPV6}/128 dst ${REMOTE_IPV6}/128 \
sport 0:65535 dport 0:65535
sp ipv6 out esp bypass pri 1 sport 0:65535 dport 0:65535

#SA in rules
sa in 7 cipher_algo 3des-cbc \
cipher_key \
de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
auth_algo sha1-hmac \
auth_key de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
mode transport ${SGW_CFG_XPRM_IN}

sa in 9 cipher_algo 3des-cbc \
cipher_key \
de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
auth_algo sha1-hmac \
auth_key de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
mode transport ${SGW_CFG_XPRM_IN}

#SA out rules
sa out 7 cipher_algo 3des-cbc \
cipher_key \
de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
auth_algo sha1-hmac \
auth_key de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
mode transport ${SGW_CFG_XPRM_OUT}

#SA out rules
sa out 9 cipher_algo 3des-cbc \
cipher_key \
de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
auth_algo sha1-hmac \
auth_key de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef:de:ad:be:ef \
mode transport ${SGW_CFG_XPRM_OUT}

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
