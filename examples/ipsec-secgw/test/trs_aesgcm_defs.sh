#! /bin/bash

. ${DIR}/trs_aesgcm_common_defs.sh

SGW_CMD_XPRM='-w 300'

config_remote_xfrm()
{
	ssh ${REMOTE_HOST} ip xfrm policy flush
	ssh ${REMOTE_HOST} ip xfrm state flush

	ssh ${REMOTE_HOST} ip xfrm policy add \
src ${REMOTE_IPV4} dst ${LOCAL_IPV4} \
dir out ptype main action allow \
tmpl proto esp mode transport reqid 1

	ssh ${REMOTE_HOST} ip xfrm policy add \
src ${LOCAL_IPV4} dst ${REMOTE_IPV4} \
dir in ptype main action allow \
tmpl proto esp mode transport reqid 2

	ssh ${REMOTE_HOST} ip xfrm state add \
src ${REMOTE_IPV4} dst ${LOCAL_IPV4} \
proto esp spi 7 reqid 1 mode transport replay-window 64 \
aead "rfc4106\(gcm\(aes\)\)" \
0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef 128

	ssh ${REMOTE_HOST} ip xfrm state add \
src ${LOCAL_IPV4} dst ${REMOTE_IPV4} \
proto esp spi 7 reqid 2 mode transport replay-window 64 \
aead "rfc4106\(gcm\(aes\)\)" \
0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef 128

	ssh ${REMOTE_HOST} ip xfrm policy list
	ssh ${REMOTE_HOST} ip xfrm state list

	# to overcome problem with ipsec-secgw for inline mode,
	# when first packet(s) will be always dropped.
	# note that ping will fail here
	ssh ${REMOTE_HOST} ping -c 1 ${LOCAL_IPV4}
}

config6_remote_xfrm()
{
	config_remote_xfrm

	ssh ${REMOTE_HOST} ip xfrm policy add \
src ${REMOTE_IPV6} dst ${LOCAL_IPV6} \
dir out ptype main action allow \
tmpl proto esp mode transport reqid 3

	ssh ${REMOTE_HOST} ip xfrm policy add \
src ${LOCAL_IPV6} dst ${REMOTE_IPV6} \
dir in ptype main action allow \
tmpl proto esp mode transport reqid 4

	ssh ${REMOTE_HOST} ip xfrm state add \
src ${REMOTE_IPV6} dst ${LOCAL_IPV6} \
proto esp spi 9 reqid 3 mode transport replay-window 64 \
aead "rfc4106\(gcm\(aes\)\)" \
0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef 128

	ssh ${REMOTE_HOST} ip xfrm state add \
src ${LOCAL_IPV6} dst ${REMOTE_IPV6} \
proto esp spi 9 reqid 4 mode transport replay-window 64 \
aead "rfc4106\(gcm\(aes\)\)" \
0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef 128

	ssh ${REMOTE_HOST} ip xfrm policy list
	ssh ${REMOTE_HOST} ip xfrm state list

	# to overcome problem with ipsec-secgw for inline mode,
	# when first packet(s) will be always dropped.
	# note that ping will fail here
	ssh ${REMOTE_HOST} ping -c 1 ${LOCAL_IPV6}
}
