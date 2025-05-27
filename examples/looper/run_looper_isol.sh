#!/bin/bash

#======================================================================
##
## @@-COPYRIGHT-START-@@
##
## Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: BSD-3-Clause-Clear
##
## @@-COPYRIGHT_END-@@
##
##======================================================================
source dpdk_pmds_path.sh

# run looper
# On core 14(0x4000) if running the app on 5g server,
# On core 9(0x200) if running the app on sim server
# make sure you pick the core on numa 1, this will chnage with station type

cpu=0x4000

# Hexadecimal bitmask for ports to be enabled
# For eg:
# N=26 ports: 0x3ffffff (set this if app is used to test virt interfaces on station)
# 2 physical ports:0x3 (this will enable ehs0-->port 0 and ehs1-->port 1)(b'00000011 --> 0x3)
portMask=0x3

# select interfaces for tx and rx (output of ifconfig lists port order)
# ehs0 ---> 0
# ehs1 ---> 1
# ehs0-virt00 ---> 2
# ehs0-virt01 ---> 3
# ...
# ehs0-virt0n ---> n+2
# ehs1-virt00 ---> n+3
# ...
# ehs1-virt03 ---> n+6

txPort=0
rxPort=1

#set packet size to transmit
pktSize=100

#if running on 5g station in loopback mode
sudo ./looper ${DPDK_PMDS_PATH:+-d ${DPDK_PMDS_PATH}} \
    -c $cpu -n 2 --socket-mem=512,512 --huge-dir=/dev/hugepages1G -- \
        -p $portMask -t $txPort -r $rxPort -s $pktSize
