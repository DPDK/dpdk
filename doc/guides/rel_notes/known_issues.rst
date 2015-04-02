..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Known Issues and Limitations
============================

This section describes known issues with the DPDK software.

Unit Test for Link Bonding may fail at test_tlb_tx_burst()
----------------------------------------------------------
+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Unit Test for Link Bonding may fail at test_tlb_tx_burst()                           |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00390304                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Unit tests will fail at test_tlb_tx_burst function with error for uneven distribution|
|                                | of packets.                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Unit test link_bonding_autotest will fail                                            |
|                                |                                                                                      |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | There is no workaround available.                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Fedora 20                                                                            |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Link Bonding                                                                         |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+


Pause Frame Forwarding does not work properly on igb
----------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Pause Frame forwarding does not work properly on igb                                 |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00384637                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | For igb  devices rte_eth_flow_ctrl_set is not working as expected.                   |
|                                | Pause frames are always forwarded on igb, regardless of the RFCE, MPMCF and DPF      |
|                                | registers.                                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Pause frames will never be rejected by the host on 1G NICs and they will always be   |
|                                | forwarded.                                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | There is no workaround available.                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

In packets provided by the PMD, some flags are missing
------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | In packets provided by the PMD, some flags are missing                               |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 3                                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | In packets provided by the PMD, some flags are missing.                              |
|                                | The application does not have access to information provided by the hardware         |
|                                | (packet is broadcast, packet is multicast, packet is IPv4 and so on).                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The “ol_flags” field in the “rte_mbuf” structure is not correct and should not be    |
|                                | used.                                                                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | The application has to parse the Ethernet header itself to get the information,      |
|                                | which is slower.                                                                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

The rte_malloc library is not fully implemented
-----------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | The rte_malloc library is not fully implemented                                      |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 6                                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The rte_malloc library is not fully implemented.                                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | All debugging features of rte_malloc library described in architecture documentation |
|                                | are not yet implemented.                                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | No workaround available.                                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | rte_malloc                                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

HPET reading is slow
--------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | HPET reading is slow                                                                 |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 7                                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Reading the HPET chip is slow.                                                       |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | An application that calls “rte_get_hpet_cycles()” or “rte_timer_manage()” runs       |
|                                | slower.                                                                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | The application should not call these functions too often in the main loop.          |
|                                | An alternative is to use the TSC register through “rte_rdtsc()” which is faster,     |
|                                | but specific to an lcore and is a cycle reference, not a time reference.             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Environment Abstraction Layer (EAL)                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

HPET timers do not work on the Osage customer reference platform
----------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | HPET timers do not work on the Osage customer reference platform                     |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 17                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | HPET timers do not work on the Osage customer reference platform                     |
|                                | which includes an Intel® Xeon® processor 5500 series processor) using the            |
|                                | released BIOS from Intel.                                                            |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | On Osage boards, the implementation of the “rte_delay_us()” function must be changed |
|                                | to not use the HPET timer.                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | This can be addressed by building the system with the “CONFIG_RTE_LIBEAL_USE_HPET=n” |
|                                | configuration option or by using the --no-hpet EAL option.                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | The Osage customer reference platform.                                               |
|                                |                                                                                      |
|                                | Other vendor platforms with Intel®  Xeon® processor 5500 series processors should    |
|                                | work correctly, provided the BIOS supports HPET.                                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | lib/librte_eal/common/include/rte_cycles.h                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Not all variants of supported NIC types have been used in testing
-----------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Not all variants of supported NIC types have been used in testing                    |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 28                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The supported network interface cards can come in a number of variants with          |
|                                | different device ID's. Not all of these variants have been tested with the Intel®    |
|                                | DPDK.                                                                                |
|                                |                                                                                      |
|                                | The NIC device identifiers used during testing:                                      |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Controller XL710 for 40GbE QSFP+ [8086:1584]                     |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Controller XL710 for 40GbE QSFP+ [8086:1583]                     |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Controller X710 for 10GbE SFP+ [8086:1572]                       |
|                                |                                                                                      |
|                                | *   Intel® 82576 Gigabit Ethernet Controller [8086:10c9]                             |
|                                |                                                                                      |
|                                | *   Intel® 82576 Quad Copper Gigabit Ethernet Controller [8086:10e8]                 |
|                                |                                                                                      |
|                                | *   Intel® 82580 Dual Copper Gigabit Ethernet Controller [8086:150e]                 |
|                                |                                                                                      |
|                                | *   Intel® I350 Quad Copper Gigabit Ethernet Controller [8086:1521]                  |
|                                |                                                                                      |
|                                | *   Intel® 82599 Dual Fibre 10 Gigabit Ethernet Controller [8086:10fb]               |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Server Adapter X520-T2 [8086: 151c]                              |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Controller X540-T2 [8086:1528]                                   |
|                                |                                                                                      |
|                                | *   Intel® 82574L Gigabit Network Connection [8086:10d3]                             |
|                                |                                                                                      |
|                                | *   Emulated Intel® 82540EM Gigabit Ethernet Controller [8086:100e]                  |
|                                |                                                                                      |
|                                | *   Emulated Intel® 82545EM Gigabit Ethernet Controller [8086:100f]                  |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Server Adapter X520-4 [8086:154a]                                |
|                                |                                                                                      |
|                                | *   Intel® Ethernet Controller I210 [8086:1533]                                      |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Risk of issues with untested variants.                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | Use tested NIC variants. For those supported Ethernet controllers, additional device |
|                                | IDs may be added to the software if required.                                        |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll-mode drivers                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Multi-process sample app requires exact memory mapping
------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Multi-process sample app requires exact memory mapping                               |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 30                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The multi-process example application assumes that                                   |
|                                | it is possible to map the hugepage memory to the same virtual addresses in client    |
|                                | and server applications. Occasionally, very rarely with 64-bit, this does not occur  |
|                                | and a client application will fail on startup. The Linux                             |
|                                | “address-space layout randomization” security feature can sometimes cause this to    |
|                                | occur.                                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | A multi-process client application fails to initialize.                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | See the “Multi-process Limitations” section in the Intel®  DPDK Programmer’s Guide   |
|                                | for more information.                                                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Multi-process example application                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Packets are not sent by the 1 GbE/10 GbE SR-IOV driver when the source MAC address is not the MAC address assigned to the VF NIC
--------------------------------------------------------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Packets are not sent by the 1 GbE/10 GbE SR-IOV driver when the source MAC address   |
|                                | is not the MAC address assigned to the VF NIC                                        |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00168379                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The 1 GbE/10 GbE SR-IOV driver can only send packets when the Ethernet header’s      |
|                                | source MAC address is the same as that of the VF NIC. The reason for this is that    |
|                                | the Linux “ixgbe” driver module in the host OS has its anti-spoofing feature enabled.|
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Packets sent using the 1 GbE/10 GbE SR-IOV driver must have the source MAC address   |
|                                | correctly set to that of the VF NIC. Packets with other source address values are    |
|                                | dropped by the NIC if the application attempts to transmit them.                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Configure the Ethernet source address in each packet to match that of the VF NIC.    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | 1 GbE/10 GbE VF Poll Mode Driver (PMD)                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

SR-IOV drivers do not fully implement the rte_ethdev API
--------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | SR-IOV drivers do not fully implement the rte_ethdev API                             |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 59                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The SR-IOV drivers only supports the following rte_ethdev API functions:             |
|                                |                                                                                      |
|                                | *   rte_eth_dev_configure()                                                          |
|                                |                                                                                      |
|                                | *   rte_eth_tx_queue_setup()                                                         |
|                                |                                                                                      |
|                                | *   rte_eth_rx_queue_setup()                                                         |
|                                |                                                                                      |
|                                | *   rte_eth_dev_info_get()                                                           |
|                                |                                                                                      |
|                                | *   rte_eth_dev_start()                                                              |
|                                |                                                                                      |
|                                | *   rte_eth_tx_burst()                                                               |
|                                |                                                                                      |
|                                | *   rte_eth_rx_burst()                                                               |
|                                |                                                                                      |
|                                | *   rte_eth_dev_stop()                                                               |
|                                |                                                                                      |
|                                | *   rte_eth_stats_get()                                                              |
|                                |                                                                                      |
|                                | *   rte_eth_stats_reset()                                                            |
|                                |                                                                                      |
|                                | *   rte_eth_link_get()                                                               |
|                                |                                                                                      |
|                                | *   rte_eth_link_get_no_wait()                                                       |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Calling an unsupported function will result in an application error.                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Do not use other rte_ethdev API functions in applications that use the SR-IOV        |
|                                | drivers.                                                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | VF Poll Mode Driver (PMD)                                                            |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

PMD does not work with --no-huge EAL command line parameter
-----------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | PMD does not work with --no-huge EAL command line parameter                          |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00373461                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Currently, the DPDK does not store any information about memory allocated by         |
|                                | malloc() (for example, NUMA node, physical address), hence PMD drivers do not work   |
|                                | when the --no-huge command line parameter is supplied to EAL.                        |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Sending and receiving data with PMD will not work.                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Use huge page memory or use VFIO to map devices.                                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Systems running the DPDK on Linux                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Some hardware off-load functions are not supported by the VF Driver
-------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Some hardware off-load functions are not supported by the VF Driver                  |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00378813                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Currently, configuration of the following items is not supported by the VF driver:   |
|                                |                                                                                      |
|                                | *   IP/UDP/TCP checksum offload                                                      |
|                                |                                                                                      |
|                                | *   Jumbo Frame Receipt                                                              |
|                                |                                                                                      |
|                                | *   HW Strip CRC                                                                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Any configuration for these items in the VF register will be ignored. The behavior   |
|                                | is dependant on the current PF setting.                                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | For the PF (Physical Function) status on which the VF driver depends, there is an    |
|                                | option item under PMD in the config file. For others, the VF will keep the same      |
|                                | behavior as PF setting.                                                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | VF (SR-IOV) Poll Mode Driver (PMD)                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Kernel crash on IGB port unbinding
----------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Kernel crash on IGB port unbinding                                                   |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 74                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Kernel crash may occur                                                               |
|                                | when unbinding 1G ports from the igb_uio driver, on 2.6.3x kernels such as shipped   |
|                                | with Fedora 14.                                                                      |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Kernel crash occurs.                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Use newer kernels or do not unbind ports.                                            |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | 2.6.3x kernels such as  shipped with Fedora 14                                       |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | IGB Poll Mode Driver (PMD)                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Twinpond and Ironpond NICs do not report link status correctly
--------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Twinpond and Ironpond NICs do not report link status correctly                       |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00378800                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Twin Pond/Iron Pond NICs do not bring the physical link down when shutting down the  |
|                                | port.                                                                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The link is reported as up even after issuing "shutdown" command unless the cable is |
|                                | physically disconnected.                                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | None.                                                                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Twin Pond and Iron Pond NICs                                                         |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Discrepancies between statistics reported by different NICs
-----------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Discrepancies between statistics reported by different NICs                          |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00378113                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | Gigabit Ethernet devices from Intel include CRC bytes when calculating packet        |
|                                | reception statistics regardless of hardware CRC stripping state, while 10-Gigabit    |
|                                | Ethernet devices from Intel do so only when hardware CRC stripping is disabled.      |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | There may be a  discrepancy in how different NICs display packet reception           |
|                                | statistics.                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | None                                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Error reported opening files on DPDK initialization
---------------------------------------------------


+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Error reported opening files on DPDK initialization                                  |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 91                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | On DPDK application startup, errors may be reported when opening files as            |
|                                | part of the initialization process. This occurs if a large number, for example, 500  |
|                                | or more, or if hugepages are used, due to the per-process limit on the number of     |
|                                | open files.                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The DPDK application may fail to run.                                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | If using 2 MB hugepages, consider switching to a fewer number of 1 GB pages.         |
|                                | Alternatively, use the “ulimit” command to increase the number of files which can be |
|                                | opened by a process.                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Environment Abstraction Layer (EAL)                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Intel® QuickAssist Technology sample application does not work on a 32-bit OS on Shumway
----------------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Intel® QuickAssist Technology sample applications does not work on a 32- bit OS on   |
|                                | Shumway                                                                              |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 93                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The Intel® Communications Chipset 89xx Series device does not fully support NUMA on  |
|                                | a 32-bit OS. Consequently, the sample application cannot work properly on Shumway,   |
|                                | since it requires NUMA on both nodes.                                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The sample application cannot work in 32-bit mode with emulated NUMA, on             |
|                                | multi-socket boards.                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | There is no workaround available.                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Shumway                                                                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

IEEE1588 support possibly not working with an Intel® Ethernet Controller I210 NIC
---------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | IEEE1588 support may not work with an Intel® Ethernet Controller I210 NIC            |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00380285                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | IEEE1588 support is not working with an Intel® Ethernet Controller I210 NIC.         |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | IEEE1588 packets are not forwarded correctly by the Intel® Ethernet Controller I210  |
|                                | NIC.                                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | There is no workaround available.                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | IGB Poll Mode Driver                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Differences in how different Intel NICs handle maximum packet length for jumbo frame
------------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Differences in how different Intel NICs handle maximum packet length for jumbo frame |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 96                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | 10 Gigabit Ethernet devices from Intel do not take VLAN tags into account when       |
|                                | calculating packet size while Gigabit Ethernet devices do so for jumbo frames.       |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | When receiving packets with VLAN tags, the actual maximum size of useful payload     |
|                                | that Intel Gigabit Ethernet devices are able to receive is 4 bytes (or 8 bytes in    |
|                                | the case of packets with extended VLAN tags) less than that of Intel 10 Gigabit      |
|                                | Ethernet devices.                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Increase the configured maximum packet size when using Intel Gigabit Ethernet        |
|                                | devices.                                                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Binding PCI devices to igb_uio fails on Linux* kernel 3.9 when more than one device is used
-------------------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Binding PCI devices to igb_uio fails on Linux* kernel 3.9 when more than one device  |
|                                | is used                                                                              |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | 97                                                                                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | A known bug in the uio driver included in Linux* kernel version 3.9 prevents more    |
|                                | than one PCI device to be bound to the igb_uio driver.                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The Poll Mode Driver (PMD) will crash on initialization.                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Use earlier or later kernel versions, or apply the following                         |
|                                | `patch                                                                               |
|                                | <https://github.com/torvalds/linux/commit/5ed0505c713805f89473cdc0bbfb5110dfd840cb>`_|
|                                | .                                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Linux* systems with kernel version 3.9                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | igb_uio module                                                                       |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

GCC might generate Intel® AVX instructions forprocessors without Intel® AVX support
-----------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Gcc might generate Intel® AVX instructions for processors without Intel® AVX support |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00382439                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | When compiling Intel®  DPDK (and any DPDK app), gcc may generate Intel® AVX          |
|                                | instructions, even when the processor does not support Intel® AVX.                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Any DPDK app might crash while starting up.                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Either compile using icc or set EXTRA_CFLAGS=’-O3’ prior to compilation.             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Platforms which processor does not support Intel® AVX.                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Environment Abstraction Layer (EAL)                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Ethertype filter could receive other packets (non-assigned) in Niantic
----------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Ethertype filter could receive other packets (non-assigned) in Niantic               |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00169017                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | On Intel®  Ethernet Controller 82599EB:                                              |
|                                |                                                                                      |
|                                | When Ethertype filter (priority enable) was set, unmatched packets also could be     |
|                                | received on the assigned queue, such as ARP packets without 802.1q tags or with the  |
|                                | user priority not equal to set value.                                                |
|                                |                                                                                      |
|                                | Launch the testpmd by disabling RSS and with multiply queues, then add the ethertype |
|                                | filter like: “add_ethertype_filter 0 ethertype 0x0806 priority enable 3 queue 2      |
|                                | index 1”, and then start forwarding.                                                 |
|                                |                                                                                      |
|                                | When sending ARP packets without 802.1q tag and with user priority as non-3 by       |
|                                | tester, all the ARP packets can be received on the assigned queue.                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The user priority comparing in Ethertype filter cannot work probably.                |
|                                | It is the NIC's issue due to the response from PAE: “In fact, ETQF.UP is not         |
|                                | functional, and the information will be added in errata of 82599 and X540.”          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | None                                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Cannot set link speed on Intel® 40G ethernet controller
-------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Cannot set link speed on Intel® 40G ethernet controller                              |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00386379                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | On Intel® 40G Ethernet Controller:                                                   |
|                                |                                                                                      |
|                                | It cannot set the link to specific speed.                                            |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The link speed cannot be changed forcedly, though it can be configured by            |
|                                | application.                                                                         |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | None                                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Stopping the port does not down the link on Intel® 40G ethernet controller
--------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Stopping the port does not down the link on Intel® 40G ethernet controller           |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00386380                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | On Intel® 40G Ethernet Controller:                                                   |
|                                |                                                                                      |
|                                | Stopping the port does not really down the port link.                                |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The port link will be still up after stopping the port.                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | None                                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Devices bound to igb_uio with VT-d enabled do not work on Linux* kernel 3.15-3.17
---------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Devices bound to igb_uio with VT-d enabled do not work on Linux* kernel 3.15-3.17    |
+================================+======================================================================================+
| Description                    | | When VT-d is enabled (iommu=pt intel_iommu=on), devices are 1:1 mapped.            |
|                                |   In the Linux* kernel unbinding devices from drivers removes that mapping which     |
|                                |   result in IOMMU errors.                                                            |
|                                | | Introduced in Linux `kernel 3.15 commit <https://git.kernel.org/cgit/linux/kernel/ |
|                                |   git/torvalds/linux.git/commit/drivers/iommu/                                       |
|                                |   intel-iommu.c?id=816997d03bca9fabcee65f3481eb0297103eceb7>`_,                      |
|                                |   solved in Linux `kernel 3.18 commit <https://git.kernel.org/cgit/linux/kernel/git/ |
|                                |   torvalds/linux.git/commit/drivers/iommu/                                           |
|                                |   intel-iommu.c?id=1196c2fb0407683c2df92d3d09f9144d42830894>`_.                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | | Devices will not be allowed to access memory, resulting in following kernel errors:|
|                                | | ``dmar: DRHD: handling fault status reg 2``                                        |
|                                | | ``dmar: DMAR:[DMA Read] Request device [02:00.0] fault addr a0c58000``             |
|                                | | ``DMAR:[fault reason 02] Present bit in context entry is clear``                   |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | | Use earlier or later kernel versions, or avoid driver binding on boot by           |
|                                |   blacklisting the driver modules.                                                   |
|                                | | ie. in the case of ixgbe, we can pass the kernel command line option:              |
|                                | | ``modprobe.blacklist=ixgbe``                                                       |
|                                | | This way we do not need to unbind the device to bind it to igb_uio.                |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Linux* systems with kernel versions 3.15 to 3.17                                     |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | igb_uio module                                                                       |
+--------------------------------+--------------------------------------------------------------------------------------+
