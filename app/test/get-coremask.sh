#! /bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

if [ "$(uname)" = "Linux" ] ; then
	cat /sys/devices/system/cpu/present
elif [ "$(uname)" = "FreeBSD" ] ; then
	ncpus=$(/sbin/sysctl -n hw.ncpu)
	echo 0-$(expr $ncpus - 1)
else
# fallback
	echo 0-3
fi
