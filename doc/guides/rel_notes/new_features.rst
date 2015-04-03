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

New Features
============
*   Poll-mode driver support for an early release of the PCIE host interface of the Intel(R) Ethernet Switch FM10000.

    *   Basic Rx/Tx functions for PF/VF

    *   Interrupt handling support for PF/VF

    *   Per queue start/stop functions for PF/VF

    *   Support Mailbox handling between PF/VF and PF/Switch Manager

    *   Receive Side Scaling (RSS) for PF/VF

    *   Scatter receive function for PF/VF

    *   Reta update/query for PF/VF

    *   VLAN filter set for PF

    *   Link status query for PF/VF

.. note:: The software is intended to run on pre-release hardware and may contain unknown or unresolved defects or
          issues related to functionality and performance.
          The poll mode driver is also pre-release and will be updated to a released version post hardware and base driver release.
          Should the official hardware release be made between DPDK releases an updated poll-mode driver will be made available.

*   Link Bonding

    *   Support for adaptive load balancing (mode 6) to the link bonding library.

    *   Support for registration of link status change callbacks with link bonding devices.

    *   Support for slaves devices which do not support link status change interrupts in the link bonding library via a link status polling mechanism.

*   PCI Hotplug with NULL PMD sample application

*   ABI versioning

*   x32 ABI

*   Non-EAL Thread Support

*   Multi-pthread Support

*   Re-order Library

*   ACL for AVX2

*   Architecture Independent CRC Hash

*   uio_pci_generic Support

*   KNI Optimizations

*   Vhost-user support

*   Virtio (link, vlan, mac, port IO, perf)

*   IXGBE-VF RSS

*   RX/TX Callbacks

*   Unified Flow Types

*   Indirect Attached MBUF Flag

*   Use default port configuration in TestPMD

*   Tunnel offloading in TestPMD

*   Poll Mode Driver - 40 GbE Controllers (librte_pmd_i40e)

    *   Support for Flow Director

    *   Support for ethertype filter

    *   Support RSS in VF

    *   Support configuring redirection table with different size from 1GbE and 10 GbE

       -   128/512 entries of 40GbE PF

       -   64 entries of 40GbE VF

    *   Support configuring hash functions

    *   Support for VXLAN packet on Intel® 40GbE Controllers

*   Packet Distributor Sample Application

*   Job Stats library and Sample Application.

For further features supported in this release, see Chapter 3 Supported Features.
