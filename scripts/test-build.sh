#! /bin/sh -e

# BSD LICENSE
#
# Copyright 2015 6WIND S.A.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of 6WIND S.A. nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

default_path=$PATH

# Load config options:
# - AESNI_MULTI_BUFFER_LIB_PATH
# - DPDK_BUILD_TEST_CONFIGS (defconfig1+option1+option2 defconfig2)
# - DPDK_DEP_ARCHIVE
# - DPDK_DEP_CFLAGS
# - DPDK_DEP_LDFLAGS
# - DPDK_DEP_MOFED (y/[n])
# - DPDK_DEP_NUMA (y/[n])
# - DPDK_DEP_PCAP (y/[n])
# - DPDK_DEP_SSL (y/[n])
# - DPDK_DEP_SZE (y/[n])
# - DPDK_DEP_ZLIB (y/[n])
# - DPDK_MAKE_JOBS (int)
# - DPDK_NOTIFY (notify-send)
# - LIBSSO_SNOW3G_PATH
# - LIBSSO_KASUMI_PATH
. $(dirname $(readlink -e $0))/load-devel-config.sh

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
	        -s    short test with only first config without examples/doc
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
	unset DPDK_DEP_LDFLAGS
	unset DPDK_DEP_MOFED
	unset DPDK_DEP_NUMA
	unset DPDK_DEP_PCAP
	unset DPDK_DEP_SSL
	unset DPDK_DEP_SZE
	unset DPDK_DEP_ZLIB
	unset AESNI_MULTI_BUFFER_LIB_PATH
	unset LIBSSO_SNOW3G_PATH
	unset LIBSSO_KASUMI_PATH
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
		sed -ri     's,(RTE_LOG_LEVEL=).*,\1RTE_LOG_DEBUG,' $1/.config
		sed -ri           's,(_DEBUG.*=)n,\1y,' $1/.config
		sed -ri            's,(_STAT.*=)n,\1y,' $1/.config
		sed -ri 's,(TEST_PMD_RECORD_.*=)n,\1y,' $1/.config )

		# Automatic configuration
		test "$DPDK_DEP_NUMA" != y || \
		sed -ri               's,(NUMA=)n,\1y,' $1/.config
		sed -ri    's,(LIBRTE_IEEE1588=)n,\1y,' $1/.config
		sed -ri             's,(BYPASS=)n,\1y,' $1/.config
		test "$DPDK_DEP_ARCHIVE" != y || \
		sed -ri       's,(RESOURCE_TAR=)n,\1y,' $1/.config
		test "$DPDK_DEP_MOFED" != y || \
		sed -ri           's,(MLX._PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_SZE" != y || \
		sed -ri       's,(PMD_SZEDATA2=)n,\1y,' $1/.config
		test "$DPDK_DEP_ZLIB" != y || \
		sed -ri          's,(BNX2X_PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_ZLIB" != y || \
		sed -ri           's,(QEDE_PMD=)n,\1y,' $1/.config
		sed -ri            's,(NFP_PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_PCAP" != y || \
		sed -ri               's,(PCAP=)n,\1y,' $1/.config
		test -z "$AESNI_MULTI_BUFFER_LIB_PATH" || \
		sed -ri       's,(PMD_AESNI_MB=)n,\1y,' $1/.config
		test -z "$AESNI_MULTI_BUFFER_LIB_PATH" || \
		sed -ri      's,(PMD_AESNI_GCM=)n,\1y,' $1/.config
		test -z "$LIBSSO_SNOW3G_PATH" || \
		sed -ri         's,(PMD_SNOW3G=)n,\1y,' $1/.config
		test -z "$LIBSSO_KASUMI_PATH" || \
		sed -ri         's,(PMD_KASUMI=)n,\1y,' $1/.config
		test "$DPDK_DEP_SSL" != y || \
		sed -ri            's,(PMD_QAT=)n,\1y,' $1/.config
		sed -ri        's,(KNI_VHOST.*=)n,\1y,' $1/.config
		sed -ri           's,(SCHED_.*=)n,\1y,' $1/.config
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
	. $(dirname $(readlink -e $0))/load-devel-config.sh

	options=$(echo $conf | sed 's,[^~+]*,,')
	dir=$conf
	config $dir $target $options

	echo "================== Build $dir"
	make -j$J EXTRA_CFLAGS="$maxerr $DPDK_DEP_CFLAGS" \
		EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS" $verbose O=$dir
	! $short || break
	echo "================== Build examples for $dir"
	export RTE_SDK=$(pwd)
	export RTE_TARGET=$dir
	make -j$J -sC examples \
		EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS" $verbose \
		O=$(readlink -m $dir/examples)
	! echo $target | grep -q '^x86_64' || \
	make -j$J -sC examples/performance-thread \
		EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS" $verbose \
		O=$(readlink -m $dir/examples/performance-thread)
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
