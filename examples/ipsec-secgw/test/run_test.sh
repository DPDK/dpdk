#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

# Usage: /bin/bash run_test.sh [-46miflscph] <ipsec_mode>
# Run all defined linux_test.sh test-cases one by one
# If <ipsec_mode> is specified, run only that test case
# User has to setup properly the following environment variables:
#  SGW_PATH	- path to the ipsec-secgw binary to test
#  REMOTE_HOST	- ip/hostname of the DUT
#  REMOTE_IFACE	- iface name for the test-port on DUT
#  ETH_DEV	- ethernet device to be used on SUT by DPDK ('-a <pci-id>')
# Also user can optionally setup:
#  SGW_LCORE	- lcore to run ipsec-secgw on (default value is 0)
#  CRYPTO_DEV	- crypto device to be used ('-a <pci-id>')
#	       if none specified appropriate vdevs will be created by the script
#  SGW_MULTI_SEG - ipsec-secgw option to enable reassembly support and
#		specify size of reassembly table (i.e. SGW_MULTI_SEG=128)
# Refer to linux_test.sh for more information

# All supported modes to test:
#  trs_3descbc_sha1
#  trs_aescbc_sha1
#  trs_aesctr_sha1
#  trs_aesgcm
#  tun_3descbc_sha1
#  tun_aescbc_sha1
#  tun_aesctr_sha1
#  tun_aesgcm
# Naming convention:
# 'tun/trs' refer to tunnel/transport mode respectively

usage()
{
	echo "Usage:"
	echo -e "\t$0 -[46miflscph] <ipsec_mode>"
	echo -e "\t\t-4 Perform Linux IPv4 network tests"
	echo -e "\t\t-6 Perform Linux IPv6 network tests"
	echo -e "\t\t-m Add mixed IP protocol tests to IPv4/IPv6 \
(only with option [-46])"
	echo -e "\t\t-i Run inline tests (only with option [-46])"
	echo -e "\t\t-f Run fallback tests (only with option [-46])"
	echo -e "\t\t-l Run tests in legacy mode"
	echo -e "\t\t-s Run all tests with reassembly support \
(on default only fallback tests use reassembly support)"
	echo -e "\t\t-c Run tests with use of cpu-crypto \
(on default lookaside-none is used)"
	echo -e "\t\t-p Perform packet validation tests"
	echo -e "\t\t-h Display this help"
	echo -e "\t\t<ipsec_mode> Run only specified test case i.e. tun_aesgcm"
}

LINUX_TEST="trs_3descbc_sha1 \
trs_aescbc_sha1 \
trs_aesctr_sha1 \
trs_aesgcm \
tun_3descbc_sha1 \
tun_aescbc_sha1 \
tun_aesctr_sha1 \
tun_aesgcm"

LINUX_TEST_INLINE_FALLBACK="trs_aesgcm \
tun_aesgcm"

LINUX_TEST_RUN=""

PKT_TESTS="trs_ipv6opts \
tun_null_header_reconstruct"

DIR=$(dirname $0)

# get input options
run4=0
run6=0
runpkt=0
mixed=0
inline=0
fallback=0
legacy=0
multi_seg=0
cpu_crypto=0
options=""
while getopts ":46miflscph" opt
do
	case $opt in
		4)
			run4=1
			;;
		6)
			run6=1
			;;
		m)
			mixed=1
			;;
		i)
			inline=1
			;;
		f)
			fallback=1
			;;
		l)
			legacy=1
			options="${options} -l"
			;;
		s)
			multi_seg=1
			options="${options} -s"
			;;
		c)
			cpu_crypto=1
			options="${options} -c"
			;;
		p)
			runpkt=1
			;;
		h)
			usage
			exit 0
			;;
		?)
			echo "Invalid option"
			usage
			exit 127
			;;
	esac
done

shift $((OPTIND -1))
LINUX_TEST_RUN=$*

# no test suite has been selected
if [[ ${run4} -eq 0 && ${run6} -eq 0 && ${runpkt} -eq 0 ]]; then
	usage
	exit 127
fi

# check parameters
if [[ ${legacy} -eq 1 ]] && [[ ${multi_seg} -eq 1 || ${fallback} -eq 1 \
   || ${cpu_crypto} -eq 1 ]]; then
	echo "Fallback/reassembly/cpu-crypto cannot be used with legacy mode"
	exit 127
fi

if [[ ${cpu_crypto} -eq 1 && ${inline} -eq 1 && ${fallback} -eq 0 ]]; then
	echo "cpu-crypto cannot be used with inline mode"
	exit 127
fi

# perform packet processing validation tests
st=0
if [ $runpkt -eq 1 ]; then
	echo "Performing packet validation tests"
	/bin/bash ${DIR}/pkttest.sh ${PKT_TESTS}
	st=$?

	echo "pkttests finished with status ${st}"
	if [[ ${st} -ne 0 ]]; then
		echo "ERROR pkttests FAILED"
		exit ${st}
	fi
fi

desc=""

# set inline/fallback tests if needed
if [[ ${inline} -eq 1  || ${fallback} -eq 1 ]]; then

	# add inline option if needed
	if [[ ${inline} -eq 1 ]]; then
		options="${options} -i"
		desc="inline"
	fi
	# add fallback option if needed
	if [[ ${fallback} -eq 1 ]]; then
		options="${options} -f"
		if [[ "${desc}" == "inline" ]]; then
			desc="${desc} and fallback"
		else
			desc="fallback"
		fi
	fi

	# select tests to run
	if [[ -z "${LINUX_TEST_RUN}" ]]; then
		LINUX_TEST_RUN="${LINUX_TEST_INLINE_FALLBACK}"
	fi
else
	options="${options} -r"
fi

# select tests to run
if [[ -z "${LINUX_TEST_RUN}" ]]; then
	LINUX_TEST_RUN="${LINUX_TEST}"
fi

# perform selected tests
if [[ ${run4} -eq 1 || ${run6} -eq 1 ]] ; then

	for i in ${LINUX_TEST_RUN}; do

		echo "starting ${desc} test ${i}"

		st4=0
		st4m=0
		if [[ ${run4} -ne 0 ]]; then
			/bin/bash ${DIR}/load_env.sh ${options} ipv4-ipv4 ${i}
			st4=$?
			echo "${desc} test IPv4 ${i} finished with status \
${st4}"
			if [[ ${mixed} -ne 0 ]] && [[ "${i}" == tun* ]]; then
				/bin/bash ${DIR}/load_env.sh ${options} \
				ipv4-ipv6 ${i}
				st4m=$?
				echo "${desc} test IPv4-IPv6 ${i} finished with\
 status ${st4m}"
			fi
		fi

		st6=0
		st6m=0
		if [[ ${run6} -ne 0 ]]; then
			/bin/bash ${DIR}/load_env.sh ${options} ipv6-ipv6 ${i}
			st6=$?
			echo "${desc} test IPv6 ${i} finished with status \
${st6}"
			if [[ ${mixed} -ne 0 ]] && [[ "${i}" == tun* ]]; then
				/bin/bash ${DIR}/load_env.sh ${options} \
				ipv6-ipv4 ${i}
				st6m=$?
				echo "${desc} test IPv6-IPv4 ${i} finished with\
 status ${st6m}"
			fi
		fi

		let "st = st4 + st6 + st4m + st6m"
		if [[ $st -ne 0 ]]; then
			echo "ERROR ${desc} test ${i} FAILED"
			exit $st
		fi
	done
fi

echo "All tests have ended successfully"
