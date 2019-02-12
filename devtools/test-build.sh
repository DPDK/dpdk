#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015 6WIND S.A.

default_path=$PATH

# Load config options:
# - ARMV8_CRYPTO_LIB_PATH
# - DPDK_BUILD_TEST_CONFIGS (defconfig1+option1+option2 defconfig2)
# - DPDK_DEP_ARCHIVE
# - DPDK_DEP_CFLAGS
# - DPDK_DEP_ELF (y/[n])
# - DPDK_DEP_ISAL (y/[n])
# - DPDK_DEP_JSON (y/[n])
# - DPDK_DEP_LDFLAGS
# - DPDK_DEP_MLX (y/[n])
# - DPDK_DEP_NUMA ([y]/n)
# - DPDK_DEP_PCAP (y/[n])
# - DPDK_DEP_SSL (y/[n])
# - DPDK_DEP_IPSEC_MB (y/[n])
# - DPDK_DEP_SZE (y/[n])
# - DPDK_DEP_ZLIB (y/[n])
# - DPDK_MAKE_JOBS (int)
# - DPDK_NOTIFY (notify-send)
# - FLEXRAN_SDK
# - LIBMUSDK_PATH
# - LIBSSO_SNOW3G_PATH
# - LIBSSO_KASUMI_PATH
# - LIBSSO_ZUC_PATH
. $(dirname $(readlink -e $0))/load-devel-config

print_usage () {
	echo "usage: $(basename $0) [-h] [-jX] [-s] [config1 [config2] ...]]"
}

print_help () {
	echo 'Test building several targets with different options'
	echo
	print_usage
	cat <<- END_OF_HELP

	options:
	        -h    this help
	        -jX   use X parallel jobs in "make"
	        -s    short test only first config without tests|examples|doc
	        -v    verbose build

	config: defconfig[[~][+]option1[[~][+]option2...]]
	        Example: x86_64-native-linuxapp-gcc+debug~RXTX_CALLBACKS
	        The lowercase options are defined inside $(basename $0).
	        The uppercase options can be the end of a defconfig option
	        to enable if prefixed with '+' or to disable if prefixed with '~'.
	        Default is to automatically enable most of the options.
	        The external dependencies are setup with DPDK_DEP_* variables.
	        If no config on command line, DPDK_BUILD_TEST_CONFIGS is used.
	END_OF_HELP
}

J=$DPDK_MAKE_JOBS
short=false
unset verbose
maxerr=-Wfatal-errors
while getopts hj:sv ARG ; do
	case $ARG in
		j ) J=$OPTARG ;;
		s ) short=true ;;
		v ) verbose='V=1' ;;
		h ) print_help ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))
configs=${*:-$DPDK_BUILD_TEST_CONFIGS}

success=false
on_exit ()
{
	if $success ; then
		[ "$DPDK_NOTIFY" != notify-send ] || \
			notify-send -u low --icon=dialog-information 'DPDK build' 'finished'
	elif [ -z "$signal" ] ; then
		[ -z "$dir" ] || echo "failed to build $dir" >&2
		[ "$DPDK_NOTIFY" != notify-send ] || \
			notify-send -u low --icon=dialog-error 'DPDK build' 'failed'
	fi
}
# catch manual interrupt to ignore notification
trap "signal=INT ; trap - INT ; kill -INT $$" INT
# notify result on exit
trap on_exit EXIT

cd $(dirname $(readlink -m $0))/..

reset_env ()
{
	export PATH=$default_path
	unset CROSS
	unset DPDK_DEP_ARCHIVE
	unset DPDK_DEP_CFLAGS
	unset DPDK_DEP_ELF
	unset DPDK_DEP_ISAL
	unset DPDK_DEP_JSON
	unset DPDK_DEP_LDFLAGS
	unset DPDK_DEP_MLX
	unset DPDK_DEP_NUMA
	unset DPDK_DEP_PCAP
	unset DPDK_DEP_SSL
	unset DPDK_DEP_IPSEC_MB
	unset DPDK_DEP_SZE
	unset DPDK_DEP_ZLIB
	unset ARMV8_CRYPTO_LIB_PATH
	unset FLEXRAN_SDK
	unset LIBMUSDK_PATH
	unset LIBSSO_SNOW3G_PATH
	unset LIBSSO_KASUMI_PATH
	unset LIBSSO_ZUC_PATH
	unset PQOS_INSTALL_PATH
}

config () # <directory> <target> <options>
{
	reconfig=false
	if git rev-parse 2>&- && [ -n "$(git diff HEAD~ -- config)" ] ; then
		echo 'Default config may have changed'
		reconfig=true
	fi
	if [ ! -e $1/.config ] || $reconfig ; then
		echo "================== Configure $1"
		make T=$2 O=$1 config

		echo 'Customize configuration'
		# Built-in options (lowercase)
		! echo $3 | grep -q '+default' || \
		sed -ri 's,(RTE_MACHINE=")native,\1default,' $1/.config
		echo $3 | grep -q '+next' || \
		sed -ri           's,(NEXT_ABI=)y,\1n,' $1/.config
		! echo $3 | grep -q '+shared' || \
		sed -ri         's,(SHARED_LIB=)n,\1y,' $1/.config
		! echo $3 | grep -q '+debug' || ( \
		sed -ri  's,(RTE_LOG_DP_LEVEL=).*,\1RTE_LOG_DEBUG,' $1/.config
		sed -ri           's,(_DEBUG.*=)n,\1y,' $1/.config
		sed -ri            's,(_STAT.*=)n,\1y,' $1/.config
		sed -ri 's,(TEST_PMD_RECORD_.*=)n,\1y,' $1/.config )

		# Automatic configuration
		test "$DPDK_DEP_NUMA" != n || \
		sed -ri             's,(NUMA.*=)y,\1n,' $1/.config
		sed -ri    's,(LIBRTE_IEEE1588=)n,\1y,' $1/.config
		sed -ri             's,(BYPASS=)n,\1y,' $1/.config
		test "$DPDK_DEP_ARCHIVE" != y || \
		sed -ri       's,(RESOURCE_TAR=)n,\1y,' $1/.config
		test "$DPDK_DEP_ISAL" != y || \
		sed -ri           's,(PMD_ISAL=)n,\1y,' $1/.config
		test "$DPDK_DEP_MLX" != y || \
		sed -ri           's,(MLX._PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_SZE" != y || \
		sed -ri       's,(PMD_SZEDATA2=)n,\1y,' $1/.config
		test "$DPDK_DEP_ZLIB" != y || \
		sed -ri          's,(BNX2X_PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_ZLIB" != y || \
		sed -ri           's,(PMD_ZLIB=)n,\1y,' $1/.config
		test "$DPDK_DEP_ZLIB" != y || \
		sed -ri   's,(COMPRESSDEV_TEST=)n,\1y,' $1/.config
		test "$DPDK_DEP_PCAP" != y || \
		sed -ri               's,(PCAP=)n,\1y,' $1/.config
		test -z "$ARMV8_CRYPTO_LIB_PATH" || \
		sed -ri   's,(PMD_ARMV8_CRYPTO=)n,\1y,' $1/.config
		test "$DPDK_DEP_IPSEC_MB" != y || \
		sed -ri       's,(PMD_AESNI_MB=)n,\1y,' $1/.config
		test "$DPDK_DEP_IPSEC_MB" != y || \
		sed -ri      's,(PMD_AESNI_GCM=)n,\1y,' $1/.config
		test -z "$LIBSSO_SNOW3G_PATH" || \
		sed -ri         's,(PMD_SNOW3G=)n,\1y,' $1/.config
		test -z "$LIBSSO_KASUMI_PATH" || \
		sed -ri         's,(PMD_KASUMI=)n,\1y,' $1/.config
		test -z "$LIBSSO_ZUC_PATH" || \
		sed -ri            's,(PMD_ZUC=)n,\1y,' $1/.config
		test "$DPDK_DEP_SSL" != y || \
		sed -ri            's,(PMD_CCP=)n,\1y,' $1/.config
		test "$DPDK_DEP_SSL" != y || \
		sed -ri        's,(PMD_OPENSSL=)n,\1y,' $1/.config
		test "$DPDK_DEP_SSL" != y || \
		sed -ri            's,(QAT_SYM=)n,\1y,' $1/.config
		test -z "$FLEXRAN_SDK" || \
		sed -ri     's,(BBDEV_TURBO_SW=)n,\1y,' $1/.config
		sed -ri           's,(SCHED_.*=)n,\1y,' $1/.config
		test -z "$LIBMUSDK_PATH" || \
		sed -ri   's,(PMD_MVSAM_CRYPTO=)n,\1y,' $1/.config
		test -z "$LIBMUSDK_PATH" || \
		sed -ri          's,(MVPP2_PMD=)n,\1y,' $1/.config
		test -z "$LIBMUSDK_PATH" || \
		sed -ri         's,(MVNETA_PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_ELF" != y || \
		sed -ri            's,(BPF_ELF=)n,\1y,' $1/.config
		test "$DPDK_DEP_JSON" != y || \
		sed -ri          's,(TELEMETRY=)n,\1y,' $1/.config
		build_config_hook $1 $2 $3

		# Explicit enabler/disabler (uppercase)
		for option in $(echo $3 | sed 's,[~+], &,g') ; do
			pattern=$(echo $option | cut -c2-)
			if echo $option | grep -q '^~' ; then
				sed -ri "s,($pattern=)y,\1n," $1/.config
			elif echo $option | grep -q '^+' ; then
				sed -ri "s,($pattern=)n,\1y," $1/.config
			fi
		done
	fi
}

# default empty hook to override in devel config
build_config_hook () # <directory> <target> <options>
{
	:
}

for conf in $configs ; do
	target=$(echo $conf | sed 's,[~+].*,,')
	# reload config with DPDK_TARGET set
	DPDK_TARGET=$target
	reset_env
	. $(dirname $(readlink -e $0))/load-devel-config

	options=$(echo $conf | sed 's,[^~+]*,,')
	dir=$conf
	config $dir $target $options

	echo "================== Build $dir"
	make -j$J EXTRA_CFLAGS="$maxerr $DPDK_DEP_CFLAGS" \
		EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS" $verbose O=$dir
	! $short || break
	echo "================== Build tests for $dir"
	make test-build -j$J EXTRA_CFLAGS="$maxerr $DPDK_DEP_CFLAGS" \
		EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS" $verbose O=$dir
	echo "================== Build examples for $dir"
	export RTE_SDK=$(pwd)
	export RTE_TARGET=$dir
	make -j$J -sC examples \
		EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS" $verbose \
		O=$(readlink -m $dir/examples)
	unset RTE_TARGET
	echo "################## $dir done."
	unset dir
done

if ! $short ; then
	mkdir -p .check
	echo "================== Build doxygen HTML API"
	make doc-api-html >/dev/null 2>.check/doc.txt
	echo "================== Build sphinx HTML guides"
	make doc-guides-html >/dev/null 2>>.check/doc.txt
	echo "================== Check docs"
	diff -u /dev/null .check/doc.txt
fi

success=true
