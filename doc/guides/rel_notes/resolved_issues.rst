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
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TOR
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Resolved Issues
===============

This section describes previously known issues that have been resolved since release version 1.2.

Running TestPMD with SRIOV in Domain U may cause it to hang when XENVIRT switch is on
-------------------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Running TestPMD with SRIOV in Domain U may cause it to hang when XENVIRT switch is on|
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00168949                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | When TestPMD is run with only SRIOV port /testpmd -c f -n 4 -- -i, the following     |
|                                | error occurs:                                                                        |
|                                |                                                                                      |
|                                | PMD: gntalloc: ioctl error                                                           |
|                                |                                                                                      |
|                                | EAL: Error - exiting with code: 1                                                    |
|                                |                                                                                      |
|                                | Cause: Creation of mbuf pool for socket 0 failed                                     |
|                                |                                                                                      |
|                                | Then, alternately run SRIOV port and virtIO with testpmd:                            |
|                                |                                                                                      |
|                                | testpmd -c f -n 4 -- -i                                                              |
|                                |                                                                                      |
|                                | testpmd -c f -n 4 --use-dev="eth_xenvirt0" -- -i                                     |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | DomU will not be accessible after you repeat this action some times                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Run testpmd with a "--total-num-mbufs=N(N<=3500)"                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Fedora 16, 64 bits + Xen hypervisor 4.2.3 + Domain 0 kernel 3.10.0                   |
|                                | +Domain U kernel 3.6.11                                                              |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | TestPMD Sample Application                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Vhost-xen cannot detect Domain U application exit on Xen version 4.0.1
----------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Vhost-xen cannot detect Domain U application exit on Xen 4.0.1.                      |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00168947                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | When using DPDK applications on Xen 4.0.1, e.g. TestPMD Sample Application,          |
|                                | on killing the application (e.g. killall testmd) vhost-switch cannot detect          |
|                                | the domain U exited and does not free the Virtio device.                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Virtio device not freed after application is killed when using vhost-switch on Xen   |
|                                |                                                                           4.0.1      |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | Resolved in DPDK 1.8                                                                 |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Xen 4.0.1                                                                            |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Vhost-switch                                                                         |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Virtio incorrect header length used if MSI-X is disabled by kernel driver
-------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Virtio incorrect header length used if MSI-X is disabled by kernel driver or         |
|                                | if VIRTIO_NET_F_MAC is not negotiated.                                               |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00384256                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The Virtio header for host-guest communication is of variable length and             |
|                                | is dependent on whether MSI-X has been enabled by the kernel driver for the network  |
|                                | device.                                                                              |
|                                |                                                                                      |
|                                | The base header length of 20 bytes will be extended by 4 bytes to accommodate MSI-X  |
|                                | vectors and the Virtio Network Device header will appear at byte offset 24.          |
|                                |                                                                                      |
|                                | The Userspace Virtio Poll Mode Driver tests the guest feature bits for the presence  |
|                                | of VIRTIO_PCI_FLAG_MISIX, however this bit field is not part of the Virtio           |
|                                | specification and resolves to the VIRTIO_NET_F_MAC feature instead.                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | The DPDK kernel driver will enable MSI-X by default,                                 |
|                                | however if loaded with "intr_mode=legacy" on a guest with a Virtio Network Device,   |
|                                | a KVM-Qemu guest may crash with the following error: "virtio-net header not in first |
|                                | element".                                                                            |
|                                |                                                                                      |
|                                | If VIRTIO_NET_F_MAC feature has not been negotiated, then the Userspace Poll Mode    |
|                                | Driver will assume that MSI-X has been disabled and will prevent the proper          |
|                                | functioning of the driver.                                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution                     | Ensure #define VIRTIO_PCI_CONFIG(hw) returns the correct offset (20 or 24 bytes) for |
|                                | the devices where in rare cases MSI-X is disabled or VIRTIO_NET_F_MAC has not been   |
|                                | negotiated.                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Virtio devices where  MSI-X is disabled or VIRTIO_NET_F_MAC feature has not been     |
|                                | negotiated.                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | librte_pmd_virtio                                                                    |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Unstable system performance across application executions with 2MB pages
------------------------------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Unstable system performance across application executions with 2MB pages             |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00372346                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | The performance of an DPDK application may vary across executions of an              |
|                                | application due to a varying number of TLB misses depending on the location of       |
|                                | accessed structures in memory.                                                       |
|                                | This situation occurs on rare occasions.                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Occasionally, relatively poor performance of DPDK applications is encountered.       |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | Using 1 GB pages results in lower usage of TLB entries, resolving this issue.        |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | Systems using 2 MB pages                                                             |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

Link status change not working with MSI interrupts
--------------------------------------------------

+--------------------------------+--------------------------------------------------------------------------------------+
| Title                          | Link status change not working with MSI interrupts                                   |
|                                |                                                                                      |
+================================+======================================================================================+
| Reference #                    | IXA00378191                                                                          |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Description                    | MSI interrupts are not supported by the PMD.                                         |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Implication                    | Link status change will only work with legacy or MSI-X interrupts.                   |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Resolution/ Workaround         | The igb_uio driver can now be loaded with either legacy or MSI-X interrupt support.  |
|                                | However, this configuration is not tested.                                           |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Affected Environment/ Platform | All                                                                                  |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+
| Driver/Module                  | Poll Mode Driver (PMD)                                                               |
|                                |                                                                                      |
+--------------------------------+--------------------------------------------------------------------------------------+

KNI does not provide Ethtool support for all NICs supported by the Poll-Mode Drivers
------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | KNI does not provide ethtool support for all NICs supported by the Poll Mode Drivers  |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Refererence #                   | IXA00383835                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | To support ethtool functionality using the KNI, the KNI libray includes seperate      |
|                                 | driver code based off the Linux kernel drivers, because this driver code is seperate  |
|                                 | from the poll-mode drivers, the set of supported NICs for these two components may    |
|                                 | differ.                                                                               |
|                                 |                                                                                       |
|                                 | Because of this, in this release, the KNI driver does not provide "ethtool" support   |
|                                 | for the Intel® Ethernet Connection I354 on the Intel Atom  Processor C2000 product    |
|                                 | Family SoCs.                                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Ethtool support with KNI will not work for NICs such as the Intel® Ethernet           |
|                                 | Connection I354. Other KNI functionality, such as injecting packets into the Linux    |
|                                 | kernel is unaffected.                                                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Updated for Intel® Ethernet Connection I354.                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Platforms using the Intel® Ethernet Connection I354 or other NICs unsupported by KNI  |
|                                 | ethtool                                                                               |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | KNI                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Linux IPv4 forwarding is not stable with vhost-switch on high packet rate
-------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Linux IPv4 forwarding is not stable with vhost-switch on high packet rate.            |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Refererence #                   | IXA00384430                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Linux IPv4 forwarding is not stable in Guest when Tx traffic is high from traffic     |
|                                 | generator using two virtio devices in VM with 10G in host.                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Packets cannot be forwarded by user space vhost-switch and Linux IPv4 forwarding if   |
|                                 | the rate of  incoming packets is greater than 1 Mpps.                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | N/A                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| AffectedEnvironment/Platform    | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Sample application                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

PCAP library overwrites mbuf data before data is used
-----------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | PCAP library overwrites mbuf data before data is used                                 |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00383976                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | PCAP library allocates 64 mbufs for reading packets from PCAP file, but declares them |
|                                 | as static and reuses the same mbufs repeatedly rather than handing off to the ring    |
|                                 | for allocation of new mbuf for each read from the PCAP file.                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | In multi-threaded applications ata in the mbuf is overwritten.                        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Fixed in eth_pcap_rx() in rte_eth_pcap.c                                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected  Environment/Platform  | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Multi-threaded applications using PCAP library                                        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

MP Client Example app - flushing part of TX is not working for some ports if set specific port mask with skipped ports
----------------------------------------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | MP  Client Example app - flushing part of TX is not working for some ports if set     |
|                                 | specific port mask with skipped ports                                                 |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 52                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | When ports not in a consecutive set, for example, ports other than ports 0, 1 or      |
|                                 | 0,1,2,3  are used with the client-service sample app, when no further packets are     |
|                                 | received by a client, the application may not flush correctly any unsent packets      |
|                                 | already buffered inside it.                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Not all buffered packets are transmitted if traffic to the clients application is     |
|                                 | stopped. While traffic is continually received for transmission on a port by a        |
|                                 | client, buffer flushing happens normally.                                             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Changed line 284 of the client.c file:                                                |
|                                 |                                                                                       |
|                                 | from "send_packets(ports);" to "send_packets(ports->id[port]);"                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Client - Server Multi-process Sample application                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Packet truncation with Intel® I350 Gigabit Ethernet Controller
--------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Packet truncation with Intel I350 Gigabit Ethernet Controller                         |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00372461                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The setting of the hw_strip_crc field in the rte_eth_conf structure passed to the     |
|                                 | rte_eth_dev_configure() function is not respected and hardware CRC stripping is       |
|                                 | always enabled.                                                                       |
|                                 | If the field is set to 0, then the software also tries to strip the CRC, resulting    |
|                                 | in packet truncation.                                                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The last 4 bytes of the packets received will be missing.                             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Fixed an omission in device initialization (setting the  STRCRC bit in the DVMOLR     |
|                                 | register) to respect the CRC stripping selection correctly.                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Systems using the Intel® I350 Gigabit Ethernet Controller                             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | 1 GbE Poll Mode Driver (PMD)                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Device initialization failure with Intel® Ethernet Server Adapter X520-T2
-------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Device initialization failure with Intel® Ethernet Server Adapter X520-T2             |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 55                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | If this device is bound to the Linux kernel IXGBE driver when the DPDK is             |
|                                 | initialized, DPDK is initialized, the device initialization fails with error code -17 |
|                                 | “IXGBE_ERR_PHY_ADDR_INVALID”.                                                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The device is not initialized and cannot be used by an application.                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Introduced a small delay in device initialization to allow DPDK to always find        |
|                                 | the device.                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Systems using the Intel® Ethernet Server Adapter X520-T2                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | 10 GbE Poll Mode Driver (PMD)                                                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

DPDK kernel module is incompatible with Linux kernel version 3.3
----------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | DPDK kernel module is incompatible with Linux kernel version 3.3                      |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373232                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The igb_uio kernel module fails to compile on systems with Linux kernel version 3.3   |
|                                 | due to API changes in kernel headers                                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The compilation fails and Ethernet controllers fail to initialize without the igb_uio |
|                                 | module.                                                                               |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Kernel functions pci_block_user_cfg_access() / pci_cfg_access_lock() and              |
|                                 | pci_unblock_user_cfg_access() / pci_cfg_access_unlock() are automatically selected at |
|                                 | compile time as appropriate.                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Linux systems using kernel version 3.3 or later                                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | UIO module                                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Initialization failure with Intel® Ethernet Controller X540-T2
--------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Initialization failure with Intel®  Ethernet Controller X540-T2                       |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 57                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | This device causes a failure during initialization when the software tries to read    |
|                                 | the part number from the device EEPROM.                                               |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Device cannot be used.                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Remove unnecessary check of the PBA number from the device.                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Systems using the Intel®  Ethernet Controller X540-T2                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | 10 GbE Poll Mode Driver (PMD)                                                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

rte_eth_dev_stop() function does not bring down the link for 1 GB NIC ports
---------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | rte_eth_dev_stop() function does not bring down the link for 1 GB NIC ports           |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373183                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | When the rte_eth_dev_stop() function is used to stop a NIC port, the link is not      |
|                                 | brought down for that port.                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Links are still reported as up, even though the NIC device has been stopped and       |
|                                 | cannot perform TX or RX operations on that port.                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | The rte_eth_dev_stop() function now brings down the link when called.                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | 1 GbE Poll Mode Driver (PMD)                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

It is not possible to adjust the duplex setting for 1GB NIC ports
-----------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | It is not possible to adjust the duplex setting for 1 GB NIC ports                    |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 66                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The rte_eth_conf structure does not have a parameter that allows a port to be set to  |
|                                 | half-duplex instead of full-duplex mode, therefore, 1 GB NICs cannot be configured    |
|                                 | explicitly to a full- or half-duplex value.                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | 1 GB port duplex capability cannot be set manually.                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | The PMD now uses a new field added to the rte_eth_conf structure to allow 1 GB ports  |
|                                 | to be configured explicitly as half- or full-duplex.                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | 1 GbE Poll Mode Driver (PMD)                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Calling rte_eth_dev_stop() on a port does not free all the mbufs in use by that port
------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Calling rte_eth_dev_stop() on a port does not free all the mbufs in use by that port  |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 67                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The rte_eth_dev_stop() function initially frees all mbufs used by that port’s RX and  |
|                                 | TX rings, but subsequently repopulates the RX ring again later in the function.       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Not all mbufs used by a port are freed when the port is stopped.                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | The driver no longer re-populates the RX ring in the rte_eth_dev_stop() function.     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IGB and IXGBE Poll Mode Drivers (PMDs)                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

PMD does not always create rings that are properly aligned in memory
--------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | PMD does not always create rings that are properly aligned in memory                  |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373158                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The NIC hardware used by the PMD requires that the RX and TX rings used must be       |
|                                 | aligned in memory on a 128-byte boundary. The memzone reservation function used       |
|                                 | inside the PMD only guarantees that the rings are aligned on a 64-byte boundary, so   |
|                                 | errors can occur if the rings are not aligned on a 128-byte boundary.                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Unintended overwriting of memory can occur and PMD behavior may also be effected.     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | A new rte_memzone_reserve_aligned() API has been added to allow memory reservations   |
|                                 | from hugepage memory at alignments other than 64-bytes. The PMD has been modified so  |
|                                 | that the rings are allocated using this API with minimum alignment of 128-bytes.      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IGB and IXGBE Poll Mode Drivers (PMDs)                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Checksum offload might not work correctly when mixing VLAN-tagged and ordinary packets
--------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Checksum offload might not work correctly when mixing VLAN-tagged and ordinary        |
|                                 | packets                                                                               |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00378372                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Incorrect handling of protocol header lengths in the PMD driver                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The checksum for one of the packets may be incorrect.                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Corrected the offset calculation.                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Poll Mode Driver (PMD)                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Port not found issue with Intel® 82580 Gigabit Ethernet Controller
------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Port not found issue with Intel® 82580 Gigabit Ethernet Controller                    |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 50                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | After going through multiple driver unbind/bind cycles, an Intel® 82580               |
|                                 | Ethernet Controller port may no longer be found and initialized by the                |
|                                 | DPDK.                                                                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The port will be unusable.                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Issue was not reproducible and therefore no longer considered an issue.               |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | 1 GbE Poll Mode Driver (PMD)                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Packet mbufs may be leaked from mempool if rte_eth_dev_start() function fails
-----------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Packet mbufs may be leaked from mempool if rte_eth_dev_start() function fails         |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373373                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The rte_eth_dev_start() function allocates mbufs to populate the NIC RX rings. If the |
|                                 | start function subsequently fails, these mbufs are not freed back to the memory pool  |
|                                 | from which they came.                                                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | mbufs may be lost to the system if rte_eth_dev_start() fails and the application does |
|                                 | not terminate.                                                                        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | mbufs are correctly deallocated if a call to rte_eth_dev_start() fails.               |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Poll Mode Driver (PMD)                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Promiscuous mode for 82580 NICs can only be enabled after a call to rte_eth_dev_start for a port
------------------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Promiscuous mode for 82580 NICs can only be enabled after a call to rte_eth_dev_start |
|                                 | for a port                                                                            |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373833                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | For 82580-based network ports, the rte_eth_dev_start() function can overwrite the     |
|                                 | setting of the promiscuous mode for the device.                                       |
|                                 |                                                                                       |
|                                 | Therefore, the rte_eth_promiscuous_enable() API call should be called after           |
|                                 | rte_eth_dev_start() for these devices.                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Promiscuous mode can only be enabled if API calls are in a specific order.            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | The NIC now restores most of its configuration after a call to rte_eth_dev_start().   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Poll Mode Driver (PMD)                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Incorrect CPU socket information reported in /proc/cpuinfo can prevent the DPDK from running
--------------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Incorrect CPU socket information reported in /proc/cpuinfo can prevent the Intel®     |
|                                 | DPDK from running                                                                     |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 63                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The DPDK users information supplied by the Linux  kernel to determine the             |
|                                 | hardware properties of the system being used. On rare occasions, information supplied |
|                                 | by /proc/cpuinfo does not match that reported elsewhere. In some cases, it has been   |
|                                 | observed that the CPU socket numbering given in /proc/cpuinfo is incorrect and this   |
|                                 | can prevent DPDK from operating.                                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The DPDK cannot run on systems where /proc/cpuinfo does not report the correct        |
|                                 | CPU socket topology.                                                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | CPU socket information is now read from /sys/devices/cpu/pcuN/topology                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Environment Abstraction Layer (EAL)                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

L3FWD sample application may fail to transmit packets under extreme conditions
------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | L3FWD sample application may fail to transmit packets under extreme conditions        |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00372919                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Under very heavy load, the L3 Forwarding sample application may fail to transmit      |
|                                 | packets due to the system running out of free mbufs.                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Sending and receiving data with the PMD may fail.                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | The number of mbufs is now calculated based on application parameters.                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | L3 Forwarding sample application                                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

L3FWD-VF might lose CRC bytes
-----------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | L3FWD-VF might lose CRC bytes                                                         |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373424                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Currently, the CRC stripping configuration does not affect the VF driver.             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Packets transmitted by the DPDK in the VM may be lacking 4 bytes (packet CRC).        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | Set “strip_crc” to 1 in the sample applications that use the VF PMD.                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IGB and IXGBE VF Poll Mode Drivers (PMDs)                                             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

32-bit DPDK sample applications fails when using more than one 1 GB hugepage
----------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | 32-bit Intel®  DPDK sample applications fails when using more than one 1 GB hugepage  |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 31                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | 32-bit applications may have problems when running with multiple 1 GB pages on a      |
|                                 | 64-bit OS. This is due to the limited address space available to 32-bit processes.    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | 32-bit processes need to use either 2 MB pages or have their memory use constrained   |
|                                 | to 1 GB if using 1 GB pages.                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | EAL now limits virtual memory to 1 GB per page size.                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | 64-bit systems running 32-bit  Intel®  DPDK with 1 GB hugepages                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Environment Abstraction Layer (EAL)                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

l2fwd fails to launch if the NIC is the Intel® 82571EB Gigabit Ethernet Controller
----------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | l2fwd fails to launch if the NIC is the Intel® 82571EB Gigabit Ethernet Controller    |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373340                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The 82571EB NIC can handle only one TX per port. The original implementation allowed  |
|                                 | for a more complex handling of multiple queues per port.                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The l2fwd  application fails to launch if the NIC is 82571EB.                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | l2fwd now uses only one TX queue.                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Sample Application                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

32-bit DPDK applications may fail to initialize on 64-bit OS
------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | 32-bit DPDK applications may fail to initialize on 64-bit OS                          |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00378513                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The EAL used a 32-bit pointer to deal with physical addresses. This could create      |
|                                 | problems when the physical address of a hugepage exceeds the 4 GB limit.              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | 32-bit applications may not initialize on a 64-bit OS.                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | The physical address pointer is now 64-bit.                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | 32-bit applications in a 64-bit Linux* environment                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Environment Abstraction Layer (EAL)                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Lpm issue when using prefixes > 24
----------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Lpm issue when using prefixes > 24                                                    |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00378395                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Extended tbl8's are overwritten by multiple lpm rule entries when the depth is        |
|                                 | greater than 24.                                                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | LPM tbl8 entries removed by additional rules.                                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | Adding tbl8 entries to a valid group to avoid making the entire table invalid and     |
|                                 | subsequently overwritten.                                                             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Sample applications                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

IXGBE PMD hangs on port shutdown when not all packets have been sent
--------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | IXGBE PMD hangs on port shutdown when not all packets have been sent                  |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373492                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | When the PMD is forwarding packets, and the link goes down, and port shutdown is      |
|                                 | called, the port cannot shutdown. Instead, it hangs due to the IXGBE driver           |
|                                 | incorrectly performing the port shutdown procedure.                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The port cannot shutdown and does not come back up until re-initialized.              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | The port shutdown procedure  has been rewritten.                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IXGBE Poll Mode Driver (PMD)                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Config file change can cause build to fail
------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Config file change can cause build to fail                                            |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00369247                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | If a change in a config file results in some DPDK files that were needed no           |
|                                 | longer being needed, the build will fail. This is because the \*.o file will still    |
|                                 | exist, and the linker will try to link it.                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | DPDK compilation failure                                                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | The Makefile now provides instructions to clean out old kernel module object files.   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Load balance sample application                                                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

rte_cmdline library should not be used in production code due to limited testing
--------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | rte_cmdline library should not be used in production code due to limited testing      |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 34                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The rte_cmdline library provides a command line interface for use in sample           |
|                                 | applications and test applications distributed as part of DPDK. However, it is        |
|                                 | not validated to the same standard as other DPDK libraries.                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | It may contain bugs or errors that could cause issues in production applications.     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | The rte_cmdline library is now tested correctly.                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | rte_cmdline                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Some \*_INITIALIZER macros are not compatible with C++
------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Some \*_INITIALIZER macros are not compatible with C++                                |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00371699                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | These macros do not work with C++ compilers, since they use the C99 method of named   |
|                                 | field initialization. The TOKEN_*_INITIALIZER macros in librte_cmdline have this      |
|                                 | problem.                                                                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | C++ application using these macros will fail to compile.                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | Macros are now compatible with C++ code.                                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | rte_timer, rte_cmdline                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

No traffic through bridge when using exception_path sample application
----------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | No traffic through bridge when using exception_path sample application                |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00168356                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | On some systems, packets are sent from the exception_path to the tap device, but are  |
|                                 | not forwarded by the bridge.                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The sample application does not work as described in its sample application quide.    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | If you cannot get packets though the bridge, it might be because IP packet filtering  |
|                                 | rules are up by default on the bridge. In that case you can disable it using the      |
|                                 | following:                                                                            |
|                                 |                                                                                       |
|                                 | # for i in /proc/sys/net/bridge/bridge_nf-\*; do echo 0 > $i; done                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Linux                                                                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Exception path sample application                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Segmentation Fault in testpmd after config fails
------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Segmentation Fault in testpmd after config fails                                      |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00378638                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Starting testpmd with a parameter that causes port queue setup to fail, for example,  |
|                                 | set TX WTHRESH to non 0 when tx_rs_thresh is greater than 1, then doing               |
|                                 | “port start all”.                                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Seg fault in testpmd                                                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | Testpmd now forces port reconfiguration if the initial configuration  failed.         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Testpmd Sample Application                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Linux kernel pci_cfg_access_lock() API can be prone to deadlock
---------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Linux kernel pci_cfg_access_lock() API can be prone to deadlock                       |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00373232                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The kernel APIs used for locking in the igb_uio driver can cause a deadlock in        |
|                                 | certain situations.                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Unknown at this time; depends on the application.                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | The igb_uio driver now uses the pci_cfg_access_trylock() function instead of          |
|                                 | pci_cfg_access_lock().                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IGB UIO Driver                                                                        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

When running multi-process applications, “rte_malloc” functions cannot be used in secondary processes
-----------------------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | When running multi-process applications, “rte_malloc” functions cannot be used in     |
|                                 | secondary processes                                                                   |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 35                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The rte_malloc library provides a set of malloc-type functions that reserve memory    |
|                                 | from hugepage shared memory. Since secondary processes cannot reserve memory directly |
|                                 | from hugepage memory, rte_malloc functions cannot be used reliably.                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The librte_malloc functions, for example, rte_malloc(), rte_zmalloc()                 |
|                                 | and rte_realloc() cannot be used reliably in secondary processes.                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | In addition to re-entrancy support, the Intel®  DPDK now supports the reservation of  |
|                                 | a memzone from the primary thread or secondary threads. This is achieved by putting   |
|                                 | the reservation-related control data structure of the memzone into shared memory.     |
|                                 | Since rte_malloc functions request memory directly from the memzone, the limitation   |
|                                 | for secondary threads no longer applies.                                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | rte_malloc                                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Configuring maximum packet length for IGB with VLAN enabled may not take intoaccount the length of VLAN tag
-----------------------------------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Configuring maximum packet length for IGB with VLAN enabled may not take into account |
|                                 | the length of VLAN tag                                                                |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00379880                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | For IGB, the maximum packet length configured may not include the length of the VLAN  |
|                                 | tag even if VLAN is enabled.                                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Packets with a VLAN tag with a size close to the maximum may be dropped.              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | NIC registers are now correctly initialized.                                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All with IGB NICs                                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IGB Poll Mode Driver (PMD)                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Intel® I210 Ethernet controller always strips CRC of incoming packets
---------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Intel® I210 Ethernet controller always strips CRC of incoming packets                 |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00380265                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The Intel® I210 Ethernet  controller (NIC) removes 4 bytes from the end of the packet |
|                                 | regardless of whether it was configured to do so or not.                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Packets will be missing 4 bytes if the NIC is not configured to strip CRC.            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/ Workaround          | NIC registers are now  correctly initialized.                                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | IGB Poll Mode Driver (PMD)                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

EAL can silently reserve less memory than requested
---------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | EAL can silently reserve less memory than requested                                   |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00380689                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | During application initialization, the EAL can silently reserve less memory than      |
|                                 | requested by the user through the -m application option.                              |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The application fails to start.                                                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | EAL will detect if this condition occurs and will give anappropriate error message    |
|                                 | describing steps to fix the problem.                                                  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Environmental Abstraction Layer (EAL)                                                 |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

SSH connectivity with the board may be lost when starting a DPDK application
----------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | SSH connectivity with the board may be lost when starting a DPDK application          |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 26                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Currently, the Intel®  DPDK takes over all the NICs found on the board that are       |
|                                 | supported by the DPDK. This results in these NICs being removed from the NIC          |
|                                 | set handled by the kernel,which has the side effect of any SSH connection being       |
|                                 | terminated. See also issue #27.                                                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Loss of network connectivity to board.                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | DPDK now no longer binds ports on startup. Please refer to the Getting Started        |
|                                 | Guide for information on how to bind/unbind ports from DPDK.                          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Systems using a Intel®DPDK supported NIC for remote system access                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Environment Abstraction Layer (EAL)                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Remote network connections lost when running autotests or sample applications
-----------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Remote network connections lost when running autotests or sample applications         |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | 27                                                                                    |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The PCI autotest and sample applications will scan for PCI devices and will remove    |
|                                 | from Linux* control those recognized by it. This may result in the loss of network    |
|                                 | connections to the system.                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Loss of network connectivity to board when connected remotely.                        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | DPDK now no longer binds ports on startup.                                            |
|                                 | Please refer to the Getting Started Guide for information on how to bind/unbind ports |
|                                 | from DPDK.                                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | Systems using a DPDK supported NIC for remote system access                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Sample applications                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

KNI may not work properly in a multi-process environment
--------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | KNI may not work properly in a multi-process environment                              |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00380475                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Some of the network interface operations such as, MTU change or link UP/DOWN, when    |
|                                 | executed on KNI interface, might fail in a multi-process environment, although they   |
|                                 | are normally successful in the DPDK single process environment.                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Some network interface operations on KNI cannot be used in a DPDK                     |
|                                 | multi-process environment.                                                            |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution                      | The ifconfig callbacks are now explicitly set in either master or secondary process.  |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Kernel Network Interface (KNI)                                                        |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Hash library cannot be used in multi-process applications with multiple binaries
--------------------------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Hash library cannot be used in multi-process applications with multiple binaries      |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00168658                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | The hash function used by a given hash-table implementation is referenced in the code |
|                                 | by way of a function pointer. This means that it cannot work in cases where the hash  |
|                                 | function is at a different location in the code segment in different processes, as is |
|                                 | the case where a DPDK multi-process application uses a number of different            |
|                                 | binaries, for example, the client-server multi-process example.                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | The Hash library will not work if shared by multiple processes.                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | New API was added for multiprocess scenario. Please refer to DPDK Programmer’s        |
|                                 | Guide for more information.                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | librte_hash library                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Unused hugepage files are not cleared after initialization
----------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Hugepage files are not cleared after initialization                                   |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00383462                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | EAL leaves hugepages allocated at initialization in the hugetlbfs even if they are    |
|                                 | not used.                                                                             |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | Reserved hugepages are not freed back to the system, preventing other applications    |
|                                 | that use hugepages from running.                                                      |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Reserved and unused hugepages are now freed back to the system.                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | EAL                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+

Packet reception issues when virtualization is enabled
------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Packet reception issues when virtualization is enabled                                |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00369908                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | Packets are not transmitted or received on when VT-d is enabled in the BIOS and Intel |
|                                 | IOMMU is used. More recent kernels do not exhibit this issue.                         |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | An application requiring packet transmission or reception will not function.          |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | DPDK Poll Mode Driver now has the ability to map correct physical addresses to        |
|                                 | the device structures.                                                                |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Poll mode drivers                                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+



Double VLAN does not work on Intel® 40GbE ethernet contoller
------------------------------------------------------------

+---------------------------------+---------------------------------------------------------------------------------------+
| Title                           | Double VLAN does not work on Intel® 40GbE ethernet controller                         |
|                                 |                                                                                       |
+=================================+=======================================================================================+
| Reference #                     | IXA00369908                                                                           |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Description                     | On Intel® 40 GbE ethernet controller double VLAN does not work.                       |
|                                 | This was confirmed as a Firmware issue which will be fixed in later versions of       |
|                                 | firmware.                                                                             |
+---------------------------------+---------------------------------------------------------------------------------------+
| Implication                     | After setting double vlan to be enabled on a port, no packets can be transmitted out  |
|                                 | on that port.                                                                         |
+---------------------------------+---------------------------------------------------------------------------------------+
| Resolution/Workaround           | Resolved in latest release with firmware upgrade.                                     |
|                                 |                                                                                       |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Affected Environment/Platform   | All                                                                                   |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
| Driver/Module                   | Poll mode drivers                                                                     |
|                                 |                                                                                       |
+---------------------------------+---------------------------------------------------------------------------------------+
