#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Red Hat, Inc.

which jq || {
    echo "No jq available, skipping test."
    exit 77
}

rootdir=$(readlink -f $(dirname $(readlink -f $0))/../..)
tmpoutput=$(mktemp -t dpdk.test_telemetry.XXXXXX)
trap "cat $tmpoutput; rm -f $tmpoutput" EXIT

call_all_telemetry() {
    telemetry_script=$rootdir/usertools/dpdk-telemetry.py
    echo >$tmpoutput
    echo "Telemetry commands log:" >>$tmpoutput
    for cmd in $(echo / | $telemetry_script | jq -r '.["/"][]')
    do
        for input in $cmd $cmd,0 $cmd,z
        do
            echo Calling $input >> $tmpoutput
            echo $input | $telemetry_script >> $tmpoutput 2>&1
        done
    done
}

(sleep 1 && call_all_telemetry && echo quit) | $@
