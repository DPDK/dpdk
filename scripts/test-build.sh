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

# Load config options:
# - DPDK_BUILD_TEST_CONFIGS (target1+option1+option2 target2)
# - DPDK_DEP_CFLAGS
# - DPDK_DEP_LDFLAGS
# - DPDK_DEP_MOFED (y/[n])
# - DPDK_DEP_PCAP (y/[n])
# - DPDK_NOTIFY (notify-send)
. scripts/load-devel-config.sh

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

	config: defconfig name followed by switches delimited with "+" sign
	        Example: x86_64-native-linuxapp-gcc+next+shared+combined
	        Default is to enable most of the options.
	        The external dependencies are setup with DPDK_DEP_* variables.
	END_OF_HELP
}

J=$DPDK_MAKE_JOBS
short=false
while getopts hj:s ARG ; do
	case $ARG in
		j ) J=$OPTARG ;;
		s ) short=true ;;
		h ) print_help ; exit 0 ;;
		? ) print_usage ; exit 1 ;;
	esac
done
shift $(($OPTIND - 1))
configs=${*:-$DPDK_BUILD_TEST_CONFIGS}

success=false
on_exit ()
{
	if [ "$DPDK_NOTIFY" = notify-send ] ; then
		if $success ; then
			notify-send -u low --icon=dialog-information 'DPDK build' 'finished'
		elif [ -z "$signal" ] ; then
			notify-send -u low --icon=dialog-error 'DPDK build' 'failed'
		fi
	fi
}
# catch manual interrupt to ignore notification
trap "signal=INT ; trap - INT ; kill -INT $$" INT
# notify result on exit
trap on_exit EXIT

cd $(dirname $(readlink -m $0))/..

config () # <directory> <target> <options>
{
	if [ ! -e $1/.config ] ; then
		echo Custom configuration
		make T=$2 O=$1 config
		echo $3 | grep -q next || \
		sed -ri           's,(NEXT_ABI=)y,\1n,' $1/.config
		! echo $3 | grep -q shared || \
		sed -ri         's,(SHARED_LIB=)n,\1y,' $1/.config
		! echo $3 | grep -q combined || \
		sed -ri       's,(COMBINE_LIBS=)n,\1y,' $1/.config
		echo $2 | grep -q '^i686' || \
		sed -ri               's,(NUMA=)n,\1y,' $1/.config
		sed -ri         's,(PCI_CONFIG=)n,\1y,' $1/.config
		sed -ri    's,(LIBRTE_IEEE1588=)n,\1y,' $1/.config
		sed -ri             's,(BYPASS=)n,\1y,' $1/.config
		test "$DPDK_DEP_MOFED" != y || \
		echo $2 | grep -q '^clang$' || \
		echo $3 | grep -q 'shared.*combined' || \
		sed -ri           's,(MLX._PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_SZE" != y || \
		echo $2 | grep -q '^i686' || \
		sed -ri       's,(PMD_SZEDATA2=)n,\1y,' $1/.config
		test "$DPDK_DEP_ZLIB" != y || \
		sed -ri          's,(BNX2X_PMD=)n,\1y,' $1/.config
		sed -ri            's,(NFP_PMD=)n,\1y,' $1/.config
		test "$DPDK_DEP_PCAP" != y || \
		sed -ri               's,(PCAP=)n,\1y,' $1/.config
		test -z "$AESNI_MULTI_BUFFER_LIB_PATH" || \
		echo $2 | grep -q '^i686' || \
		echo $3 | grep -q 'shared.*combined' || \
		sed -ri       's,(PMD_AESNI_MB=)n,\1y,' $1/.config
		test "$DPDK_DEP_SSL" != y || \
		echo $2 | grep -q '^i686' || \
		echo $3 | grep -q 'shared.*combined' || \
		sed -ri            's,(PMD_QAT=)n,\1y,' $1/.config
		sed -ri        's,(KNI_VHOST.*=)n,\1y,' $1/.config
		sed -ri           's,(SCHED_.*=)n,\1y,' $1/.config
		! echo $2 | grep -q '^i686' || \
		sed -ri              's,(POWER=)y,\1n,' $1/.config
		sed -ri 's,(TEST_PMD_RECORD_.*=)n,\1y,' $1/.config
		sed -ri            's,(DEBUG.*=)n,\1y,' $1/.config
	fi
}

for conf in $configs ; do
	target=$(echo $conf | cut -d'+' -f1)
	options=$(echo $conf | cut -d'+' -sf2- --output-delimiter='-')
	if [ -z "$options" ] ; then
		dir=$target
		config $dir $target
		# Use install rule
		make -j$J T=$target install EXTRA_CFLAGS="$DPDK_DEP_CFLAGS" EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS"
		$short || make -j$J T=$target examples O=$dir/examples EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS"
	else
		dir=$target-$options
		config $dir $target $options
		echo "================== Build $dir"
		# Use O variable without install
		make -j$J O=$dir EXTRA_CFLAGS="$DPDK_DEP_CFLAGS" EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS"
		echo "================== Build examples for $dir"
		make -j$J -sC examples RTE_SDK=$(pwd) RTE_TARGET=$dir O=$(readlink -m $dir/examples) EXTRA_LDFLAGS="$DPDK_DEP_LDFLAGS"
	fi
	echo "################## $dir done."
	! $short || break
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
