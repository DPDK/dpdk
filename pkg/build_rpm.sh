#!/bin/bash

set -e -x

name=dpdk-vhost-vfe
topdir=/tmp/build_dpdk_topdir
specfile=pkg/$name.spec
release=`rpmspec --srpm  -q --queryformat='%{RELEASE}\n' $specfile`
ver=`rpmspec --srpm  -q --queryformat='%{VERSION}\n' $specfile`

srctar=$name-$ver.tar.gz
srcrpm=$name-$ver-$release.src.rpm
binrpm=$name-$ver-$release.`uname -m`.rpm
srcrpm_full=$topdir/SRPMS/$srcrpm
binrpm_full=$topdir/RPMS/`uname -m`/$binrpm

mkdir -p ${topdir}/{SOURCES,RPMS,SRPMS,SPECS,BUILD,BUILDROOT}

# generate src.rpm
if [[ ! -f $srcrpm ]] ; then
	echo "====  building source rpm package $srcrpm ===="

	rm -rf $topdir/SOURCES/$srctar
	git archive \
		--format=tar.gz --prefix=$name-$ver/ -o $topdir/SOURCES/$srctar HEAD

	rpmbuild -bs --define "_topdir $topdir" $specfile

	if [ -f $srcrpm_full ]; then
		echo "$srcrpm build success"
		ls -l $srcrpm_full
	else
		echo "$srcrpm build fail"
	fi

	cp $srcrpm_full .

else
	echo -e "$srcrpm exists, rm it to regenerate"
fi

# build rpm
if [[ ! -f $binrpm ]] ; then
	echo "====  building $binrpm ===="

	rpmbuild --rebuild --define "_topdir $topdir" $srcrpm_full

	if [ -f $binrpm_full ]; then
		echo "$binrpm build success"
		ls -l $binrpm_full
	else
		echo "$binrpm build fail"
	fi

	cp $binrpm_full .
else
	echo -e "$binrpm exists, rm it to regenerate"
fi

