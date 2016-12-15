#!/bin/sh
#   BSD LICENSE
#
#   Copyright(c) 2015 Neil Horman. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

TAG1=$1
TAG2=$2
TARGET=$3
ABI_DIR=`mktemp -d -p /tmp ABI.XXXXXX`

usage() {
	echo "$0 <REV1> <REV2> <TARGET>"
}

log() {
	local level=$1
	shift
	echo "$*"
}

validate_tags() {

	if [ -z "$HASH1" ]
	then
		echo "invalid revision: $TAG1"
		return
	fi
	if [ -z "$HASH2" ]
	then
		echo "invalid revision: $TAG2"
		return
	fi
}

validate_args() {
	if [ -z "$TAG1" ]
	then
		echo "Must Specify REV1"
		return
	fi
	if [ -z "$TAG2" ]
	then
		echo "Must Specify REV2"
		return
	fi
	if [ -z "$TARGET" ]
	then
		echo "Must Specify a build target"
	fi
}


cleanup_and_exit() {
	rm -rf $ABI_DIR
	git checkout $CURRENT_BRANCH
	exit $1
}

# Make sure we configure SHARED libraries
# Also turn off IGB and KNI as those require kernel headers to build
fixup_config() {
	sed -i -e"$ a\CONFIG_RTE_BUILD_SHARED_LIB=y" config/defconfig_$TARGET
	sed -i -e"$ a\CONFIG_RTE_NEXT_ABI=n" config/defconfig_$TARGET
	sed -i -e"$ a\CONFIG_RTE_EAL_IGB_UIO=n" config/defconfig_$TARGET
	sed -i -e"$ a\CONFIG_RTE_LIBRTE_KNI=n" config/defconfig_$TARGET
	sed -i -e"$ a\CONFIG_RTE_KNI_KMOD=n" config/defconfig_$TARGET
}

###########################################
#START
############################################

#trap on ctrl-c to clean up
trap cleanup_and_exit SIGINT

if [ -z "$DPDK_MAKE_JOBS" ]
then
	# This counts the number of cpus on the system
	if [ -e /usr/bin/lscpu ]
	then
		DPDK_MAKE_JOBS=`lscpu -p=cpu | grep -v "#" | wc -l`
	else
		DPDK_MAKE_JOBS=1
	fi
fi

#Save the current branch
CURRENT_BRANCH=`git branch | grep \* | cut -d' ' -f2`

if [ -z "$CURRENT_BRANCH" ]
then
	CURRENT_BRANCH=`git log --pretty=format:%H HEAD~1..HEAD`
fi

if [ -n "$VERBOSE" ]
then
	export VERBOSE=/dev/stdout
else
	export VERBOSE=/dev/null
fi

# Validate that we have all the arguments we need
res=$(validate_args)
if [ -n "$res" ]
then
	echo $res
	usage
	cleanup_and_exit 1
fi

HASH1=$(git show -s --format=%H "$TAG1" -- 2> /dev/null | tail -1)
HASH2=$(git show -s --format=%H "$TAG2" -- 2> /dev/null | tail -1)

# Make sure our tags exist
res=$(validate_tags)
if [ -n "$res" ]
then
	echo $res
	cleanup_and_exit 1
fi

# Make hashes available in output for non-local reference
TAG1="$TAG1 ($HASH1)"
TAG2="$TAG2 ($HASH2)"

ABICHECK=`which abi-compliance-checker 2>/dev/null`
if [ $? -ne 0 ]
then
	log "INFO" "Cant find abi-compliance-checker utility"
	cleanup_and_exit 1
fi

ABIDUMP=`which abi-dumper 2>/dev/null`
if [ $? -ne 0 ]
then
	log "INFO" "Cant find abi-dumper utility"
	cleanup_and_exit 1
fi

log "INFO" "We're going to check and make sure that applications built"
log "INFO" "against DPDK DSOs from version $TAG1 will still run when executed"
log "INFO" "against DPDK DSOs built from version $TAG2."
log "INFO" ""

# Check to make sure we have a clean tree
git status | grep -q clean
if [ $? -ne 0 ]
then
	log "WARN" "Working directory not clean, aborting"
	cleanup_and_exit 1
fi

# Move to the root of the git tree
cd $(dirname $0)/..

log "INFO" "Checking out version $TAG1 of the dpdk"
# Move to the old version of the tree
git checkout $HASH1

fixup_config

# Checking abi compliance relies on using the dwarf information in
# The shared objects.  Thats only included in the DSO's if we build
# with -g
export EXTRA_CFLAGS="$EXTRA_CFLAGS -g -O0"
export EXTRA_LDFLAGS="$EXTRA_LDFLAGS -g"

# Now configure the build
log "INFO" "Configuring DPDK $TAG1"
make config T=$TARGET O=$TARGET > $VERBOSE 2>&1

log "INFO" "Building DPDK $TAG1. This might take a moment"
make -j$DPDK_MAKE_JOBS O=$TARGET > $VERBOSE 2>&1

if [ $? -ne 0 ]
then
	log "INFO" "THE BUILD FAILED.  ABORTING"
	cleanup_and_exit 1
fi

# Move to the lib directory
cd $TARGET/lib
log "INFO" "COLLECTING ABI INFORMATION FOR $TAG1"
for i in `ls *.so`
do
	$ABIDUMP $i -o $ABI_DIR/$i-ABI-0.dump -lver $HASH1
done
cd ../..

# Now clean the tree, checkout the second tag, and rebuild
git clean -f -d
git reset --hard
# Move to the new version of the tree
log "INFO" "Checking out version $TAG2 of the dpdk"
git checkout $HASH2

fixup_config

# Now configure the build
log "INFO" "Configuring DPDK $TAG2"
make config T=$TARGET O=$TARGET > $VERBOSE 2>&1

log "INFO" "Building DPDK $TAG2. This might take a moment"
make -j$DPDK_MAKE_JOBS O=$TARGET > $VERBOSE 2>&1

if [ $? -ne 0 ]
then
	log "INFO" "THE BUILD FAILED.  ABORTING"
	cleanup_and_exit 1
fi

cd $TARGET/lib
log "INFO" "COLLECTING ABI INFORMATION FOR $TAG2"
for i in `ls *.so`
do
	$ABIDUMP $i -o $ABI_DIR/$i-ABI-1.dump -lver $HASH2
done
cd ../..

# Start comparison of ABI dumps
for i in `ls $ABI_DIR/*-1.dump`
do
	NEWNAME=`basename $i`
	OLDNAME=`basename $i | sed -e"s/1.dump/0.dump/"`
	LIBNAME=`basename $i | sed -e"s/-ABI-1.dump//"`

	if [ ! -f $ABI_DIR/$OLDNAME ]
	then
		log "INFO" "$OLDNAME DOES NOT EXIST IN $TAG1. SKIPPING..."
	fi

	#compare the abi dumps
	$ABICHECK -l $LIBNAME -old $ABI_DIR/$OLDNAME -new $ABI_DIR/$NEWNAME
done

git reset --hard
log "INFO" "ABI CHECK COMPLETE.  REPORTS ARE IN compat_report directory"
cleanup_and_exit 0
