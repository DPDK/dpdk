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

Supported Features
==================

*   Packet Distributor library for dynamic, single-packet at a time, load balancing

*   IP fragmentation and reassembly library

*   Support for IPv6 in IP fragmentation and reassembly sample applications

*   Support for VFIO for mapping BARs and setting up interrupts

*   Link Bonding PMD Library supporting round-robin, active backup, balance(layer 2, layer 2+3, and layer 3+4), broadcast bonding modes
    802.3ad link aggregation (mode 4), transmit load balancing (mode 5) and adaptive load balancing (mode 6)

*   Support zero copy mode RX/TX in user space vhost sample

*   Support multiple queues in virtio-net PMD

*   Support for Intel 40GbE Controllers:

    *   Intel® XL710 40 Gigabit Ethernet Controller

    *   Intel® X710 40 Gigabit Ethernet Controller

*   Support NIC filters in addition to flow director for Intel® 1GbE and 10GbE Controllers

*   Virtualization (KVM)

    *   Userspace vhost switch:

        New sample application to support userspace virtio back-end in host and packet switching between guests.

*   Virtualization (Xen)

    *   Support for DPDK application running on Xen Domain0 without hugepages.

    *   Para-virtualization

        Support front-end Poll Mode Driver in guest domain

        Support userspace packet switching back-end example in host domain

*   FreeBSD* 9.2 support for librte_pmd_e1000, librte_pmd_ixgbe and Virtual Function variants.
    Please refer to the *DPDK for FreeBSD\* Getting Started Guide*.
    Application support has been added for the following:

    *   multiprocess/symmetric_mp

    *   multiprocess/simple_mp

    *   l2fwd

    *   l3fwd

*   Support for sharing data over QEMU IVSHMEM

*   Support for Intel® Communications Chipset 8925 to 8955 Series in the DPDK-QAT Sample Application

*   New VMXNET3 driver for the paravirtual device presented to a VM by the VMware* ESXi Hypervisor.

*   BETA: example support for basic Netmap applications on DPDK

*   Support for the wireless KASUMI algorithm in the dpdk_qat sample application

*   Hierarchical scheduler implementing 5-level scheduling hierarchy (port, sub-port, pipe, traffic class, queue)
    with 64K leaf nodes (packet queues).

*   Packet dropper based on Random Early Detection (RED) congestion control mechanism.

*   Traffic Metering based on Single Rate Three Color Marker (srTCM) and Two Rate Three Color Marker (trTCM).

*   An API for configuring RSS redirection table on the fly

*   An API to support KNI in a multi-process environment

*   IPv6 LPM forwarding

*   Power management library and sample application using CPU frequency scaling

*   IPv4 reassembly sample application

*   Quota & Watermarks sample application

*   PCIe Multi-BAR Mapping Support

*   Support for Physical Functions in Poll Mode Driver for the following devices:

    *   Intel® 82576 Gigabit Ethernet Controller

    *   Intel® i350 Gigabit Ethernet Controller

    *   Intel® 82599 10-Gigabit Ethernet Controller

    *   Intel® XL710/X710 40-Gigabit Ethernet Controller

*   Quality of Service (QoS) Hierarchical Scheduler: Sub-port Traffic Class Oversubscription

*   Multi-thread Kernel NIC Interface (KNI) for performance improvement

*   Virtualization (KVM)

    *   Para-virtualization

        Support virtio front-end poll mode driver in guest virtual machine
        Support vHost raw socket interface as virtio back-end via KNI

    *   SR-IOV Switching for the 10G Ethernet Controller

        Support Physical Function to start/stop Virtual Function Traffic

        Support Traffic Mirroring (Pool, VLAN, Uplink and Downlink)

        Support VF multiple MAC addresses (Exact/Hash match), VLAN filtering

        Support VF receive mode configuration

*   Support VMDq for 1 GbE and 10 GbE NICs

*   Extension for the Quality of Service (QoS) sample application to allow statistics polling

*   New libpcap  -based poll-mode driver, including support for reading from 3rd Party NICs
    using Linux kernel drivers

*   New multi-process example using fork() to demonstrate application resiliency and recovery,
    including reattachment to and re-initialization of shared data structures where necessary

*   New example (vmdq) to demonstrate VLAN-based packet filtering

*   Improved scalability for scheduling large numbers of timers using the rte_timer library

*   Support for building the DPDK as a shared library

*   Support for Intel® Ethernet Server Bypass Adapter X520-SR2

*   Poll Mode Driver support for the Intel®  Ethernet Connection I354 on the Intel®  Atom™ 
    Processor C2000 Product Family SoCs

*   IPv6 exact match flow classification in the l3fwd sample application

*   Support for multiple instances of the Intel®  DPDK

*   Support for Intel®  82574L Gigabit Ethernet Controller - Intel®  Gigabit CT Desktop Adapter
    (previously code named “Hartwell”)

*   Support for Intel® Ethernet Controller I210 (previously code named “Springville”)

*   Early access support for the Quad-port Intel®  Ethernet Server Adapter X520-4 and X520-DA2
    (code named “Spring Fountain”)

*   Support for Intel®  X710/XL710 40 Gigabit Ethernet Controller (code named “Fortville”)

*   Core components:

    *   rte_mempool: allocator for fixed-sized objects

    *   rte_ring: single- or multi- consumer/producer queue implementation

    *   rte_timer: implementation of timers

    *   rte_malloc: malloc-like allocator

    *   rte_mbuf: network packet buffers, including fragmented buffers

    *   rte_hash: support for exact-match flow classification in software

    *   rte_lpm: support for longest prefix match in software for IPv4 and IPv6

    *   rte_sched: support for QoS scheduling

    *   rte_meter: support for QoS traffic metering

    *   rte_power: support for  power management

    *   rte_ip_frag: support for IP fragmentation and reassembly

*   Poll Mode Driver - Common (rte_ether)

    *   VLAN support

    *   Support for Receive Side Scaling (RSS)

    *   IEEE1588

    *   Buffer chaining; Jumbo frames

    *   TX checksum calculation

    *   Configuration of promiscuous mode, and multicast packet receive filtering

    *   L2 Mac address filtering

    *   Statistics recording

*   IGB Poll Mode Driver - 1 GbE Controllers (librte_pmd_e1000)

    *   Support for Intel® 82576 Gigabit Ethernet Controller (previously code named “Kawela”)

    *   Support for Intel® 82580 Gigabit Ethernet Controller (previously code named “Barton Hills”)

    *   Support for Intel®  I350 Gigabit Ethernet Controller (previously code named “Powerville”)

    *   Support for Intel® 82574L Gigabit Ethernet Controller - Intel® Gigabit CT Desktop Adapter
        (previously code named “Hartwell”)

    *   Support for Intel® Ethernet Controller I210 (previously code named “Springville”)

    *   Support for L2 Ethertype filters, SYN filters, 2-tuple filters and Flex filters for 82580 and i350

    *   Support for L2 Ethertype filters, SYN filters and L3/L4 5-tuple filters for 82576

*   Poll Mode Driver - 10 GbE Controllers (librte_pmd_ixgbe)

    *   Support for Intel® 82599 10 Gigabit Ethernet Controller (previously code named “Niantic”)

    *   Support for Intel® Ethernet Server Adapter X520-T2 (previously code named “Iron Pond”)

    *   Support for Intel® Ethernet Controller X540-T2 (previously code named “Twin Pond”)

    *   Support for Virtual Machine Device Queues (VMDq) and Data Center Bridging (DCB) to divide
        incoming traffic into 128 RX queues. DCB is also supported for transmitting packets.

    *   Support for auto negotiation down to 1 Gb

    *   Support for Flow Director

    *   Support for L2 Ethertype filters, SYN filters and L3/L4 5-tuple filters for 82599EB

*   Poll Mode Driver - 40 GbE Controllers (librte_pmd_i40e)

    *   Support for Intel® XL710 40 Gigabit Ethernet Controller

    *   Support for Intel® X710 40 Gigabit Ethernet Controller

*   Environment Abstraction Layer (librte_eal)

    *   Multi-process support

    *   Multi-thread support

    *   1 GB and 2 MB page support

    *   Atomic integer operations

    *   Querying CPU support of specific features

    *   High Precision Event Timer support (HPET)

    *   PCI device enumeration and blacklisting

    *   Spin locks and R/W locks

*   Test PMD application

    *   Support for PMD driver testing

*   Test application

    *   Support for core component tests

*   Sample applications

    *   Command Line

    *   Exception Path (into Linux* for packets using the Linux TUN/TAP driver)

    *   Hello World

    *   Integration with Intel® Quick Assist Technology drivers 1.0.0, 1.0.1 and 1.1.0 on Intel® 
        Communications Chipset 89xx Series C0 and C1 silicon.

    *   Link Status Interrupt (Ethernet* Link Status Detection

    *   IPv4 Fragmentation

    *   IPv4 Multicast

    *   IPv4 Reassembly

    *   L2 Forwarding (supports virtualized and non-virtualized environments)

    *   L2 Forwarding Job Stats

    *   L3 Forwarding (IPv4 and IPv6)

    *   L3 Forwarding in a Virtualized Environment

    *   L3 Forwarding with Power Management

    *   Bonding mode 6

    *   QoS Scheduling

    *   QoS Metering + Dropper

    *   Quota & Watermarks

    *   Load Balancing

    *   Multi-process

    *   Timer

    *   VMDQ and DCB L2 Forwarding

    *   Kernel NIC Interface (with ethtool support)

    *   Userspace vhost switch

*   Interactive command line interface (rte_cmdline)

*   Updated 10 GbE Poll Mode Driver (PMD) to the latest BSD code base providing support of newer
    ixgbe 10 GbE devices such as the Intel® X520-T2 server Ethernet adapter

*   An API for configuring Ethernet flow control

*   Support for interrupt-based Ethernet link status change detection

*   Support for SR-IOV functions on the Intel® 82599, Intel® 82576 and Intel® i350 Ethernet
    Controllers in a virtualized environment

*   Improvements to SR-IOV switch configurability on the Intel® 82599 Ethernet Controllers in
    a virtualized environment.

*   An API for L2 Ethernet Address “whitelist” filtering

*   An API for resetting statistics counters

*   Support for RX L4 (UDP/TCP/SCTP) checksum validation by NIC

*   Support for TX L3 (IPv4/IPv6) and L4 (UDP/TCP/SCTP) checksum calculation offloading

*   Support for IPv4 packet fragmentation and reassembly

*   Support for zero-copy Multicast

*   New APIs to allow the “blacklisting” of specific NIC ports.

*   Header files for common protocols (IP, SCTP, TCP, UDP)

*   Improved multi-process application support, allowing multiple co-operating DPDK
    processes to access the NIC port queues directly.

*   CPU-specific compiler optimization

*   Job stats library for load/cpu utilization measurements

*   Improvements to the Load Balancing sample application

*   The addition of a PAUSE instruction to tight loops for energy-usage and performance improvements

*   Updated 10 GbE Transmit architecture incorporating new upstream PCIe* optimizations.

*   IPv6 support:

    *   Support in Flow Director Signature Filters and masks

    *   RSS support in sample application that use RSS

    *   Exact match flow classification in the L3 Forwarding sample application

    *   Support in LPM for IPv6 addresses

* Tunneling packet support:

    *   Provide the APIs for VXLAN destination UDP port and VXLAN packet filter configuration
        and support VXLAN TX checksum offload on Intel® 40GbE Controllers.
