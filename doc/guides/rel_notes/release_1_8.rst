..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

DPDK Release 1.8
================

New Features
------------

*   Link Bonding

    *   Support for 802.3ad link aggregation (mode 4) and transmit load balancing (mode 5) to the link bonding library.

    *   Support for registration of link status change callbacks with link bonding devices.

    *   Support for slaves devices which do not support link status change interrupts in the link bonding library via a link status polling mechanism.

*   Poll Mode Driver - 40 GbE Controllers (librte_pmd_i40e)

    *   Support for Flow Director

    *   Support for ethertype filter

    *   Support RSS in VF

    *   Support configuring redirection table with different size from 1GbE and 10 GbE

       -   128/512 entries of 40GbE PF

       -   64 entries of 40GbE VF

    *   Support configuring hash functions

    *   Support for VXLAN packet on Intel 40GbE Controllers

*   Packet Distributor Sample Application
