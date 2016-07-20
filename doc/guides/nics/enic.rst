..  BSD LICENSE
    Copyright (c) 2015, Cisco Systems, Inc.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

ENIC Poll Mode Driver
=====================

ENIC PMD is the DPDK poll-mode driver for the Cisco System Inc. VIC Ethernet
NICs. These adapters are also referred to as vNICs below. If you are running
or would like to run DPDK software applications on Cisco UCS servers using
Cisco VIC adapters the following documentation is relevant.

How to obtain ENIC PMD integrated DPDK
--------------------------------------

ENIC PMD support is integrated into the DPDK suite. dpdk-<version>.tar.gz
should be downloaded from http://dpdk.org


Configuration information
-------------------------

- **DPDK Configuration Parameters**

  The following configuration options are available for the ENIC PMD:

  - **CONFIG_RTE_LIBRTE_ENIC_PMD** (default y): Enables or disables inclusion
    of the ENIC PMD driver in the DPDK compilation.

  - **CONFIG_RTE_LIBRTE_ENIC_DEBUG** (default n): Enables or disables debug
    logging within the ENIC PMD driver.

- **vNIC Configuration Parameters**

  - **Number of Queues**

    The maximum number of receive and transmit queues are configurable on a per
    vNIC basis through the Cisco UCS Manager (CIMC or UCSM). These values
    should be configured to be greater than or equal to the nb_rx_q and nb_tx_q
    parameters expected to  used in the call to the rte_eth_dev_configure()
    function.

  - **Size of Queues**

    Likewise, the number of receive and transmit descriptors are configurable on
    a per vNIC bases via the UCS Manager and should be greater than or equal to
    the nb_rx_desc and   nb_tx_desc parameters expected to be used in the calls
    to rte_eth_rx_queue_setup() and rte_eth_tx_queue_setup() respectively.

  - **Interrupts**

    Only one interrupt per vNIC interface should be configured in the UCS
    manager regardless of the number receive/transmit queues. The ENIC PMD
    uses this interrupt to   get information about errors in the fast path.

Limitations
-----------

- **VLAN 0 Priority Tagging**

  If a vNIC is configured in TRUNK mode by the UCS manager, the adapter will
  priority tag egress packets according to 802.1Q if they were not already
  VLAN tagged by software. If the adapter is connected to a properly configured
  switch, there will be no unexpected behavior.

  In test setups where an Ethernet port of a Cisco adapter in TRUNK mode is
  connected point-to-point to another adapter port or connected though a router
  instead of a switch, all ingress packets will be VLAN tagged. Programs such
  as l3fwd which do not account for VLAN tags in packets will misbehave. The
  solution is to enable VLAN stripping on ingress. The follow code fragment is
  example of how to accomplish this:

.. code-block:: console

     vlan_offload = rte_eth_dev_get_vlan_offload(port);
     vlan_offload |= ETH_VLAN_STRIP_OFFLOAD;
     rte_eth_dev_set_vlan_offload(port, vlan_offload);

How to build the suite?
-----------------------
The build instructions for the DPDK suite should be followed. By default
the ENIC PMD library will be built into the DPDK library.

For configuring and using UIO and VFIO frameworks, please refer the
documentation that comes with DPDK suite.

Supported Cisco VIC adapters
----------------------------

ENIC PMD supports all recent generations of Cisco VIC adapters including:

- VIC 1280
- VIC 1240
- VIC 1225
- VIC 1285
- VIC 1225T
- VIC 1227
- VIC 1227T
- VIC 1380
- VIC 1340
- VIC 1385
- VIC 1387

- Flow director features are not supported on generation 1 Cisco VIC adapters
   (M81KR and P81E)

Supported Operating Systems
---------------------------
Any Linux distribution fulfilling the conditions described in Dependencies
section of DPDK documentation.

Supported features
------------------
- Unicast, multicast and broadcast transmission and reception
- Receive queue polling
- Port Hardware Statistics
- Hardware VLAN acceleration
- IP checksum offload
- Receive side VLAN stripping
- Multiple receive and transmit queues
- Flow Director ADD, UPDATE, DELETE, STATS operation support for IPV4 5-TUPLE
  flows
- Promiscuous mode
- Setting RX VLAN (supported via UCSM/CIMC only)
- VLAN filtering (supported via UCSM/CIMC only)
- Execution of application by unprivileged system users
- IPV4, IPV6 and TCP RSS hashing
- Scattered Rx
- MTU update

Known bugs and Unsupported features in this release
---------------------------------------------------
- Signature or flex byte based flow direction
- Drop feature of flow direction
- VLAN based flow direction
- non-IPV4 flow direction
- Setting of extended VLAN
- UDP RSS hashing
- MTU update only works if Scattered Rx mode is disabled

Prerequisites
-------------
- Prepare the system as recommended by DPDK suite.  This includes environment
  variables, hugepages configuration, tool-chains and configuration
- Insert vfio-pci kernel module using the command 'modprobe vfio-pci' if the
  user wants to use VFIO framework
- Insert uio kernel module using the command 'modprobe uio' if the user wants
  to use UIO framework
- DPDK suite should be configured based on the user's decision to use VFIO or
  UIO framework
- If the vNIC device(s) to be used is bound to the kernel mode Ethernet driver
  (enic), use 'ifconfig' to bring the interface down. The dpdk-devbind.py tool
  can then be used to unbind the device's bus id from the enic kernel mode
  driver.
- Bind the intended vNIC to vfio-pci in case the user wants ENIC PMD to use
  VFIO framework using dpdk-devbind.py.
- Bind the intended vNIC to igb_uio in case the user wants ENIC PMD to use
  UIO framework using dpdk-devbind.py.

At this point the system should be ready to run DPDK applications. Once the
application runs to completion, the vNIC can be detached from vfio-pci or
igb_uio if necessary.

Root privilege is required to bind and unbind vNICs to/from VFIO/UIO.
VFIO framework helps an unprivileged user to run the applications.
For an unprivileged user to run the applications on DPDK and ENIC PMD,
it may be necessary to increase the maximum locked memory of the user.
The following command could be used to do this.

.. code-block:: console

    sudo sh -c "ulimit -l <value in Kilo Bytes>"

The value depends on the memory configuration of the application, DPDK and
PMD.  Typically, the limit has to be raised to higher than 2GB.
e.g., 2621440

The compilation of any unused drivers can be disabled using the
configuration file in config/ directory (e.g., config/common_linuxapp).
This would help in bringing down the time taken for building the
libraries and the initialization time of the application.

Additional Reference
--------------------
- http://www.cisco.com/c/en/us/products/servers-unified-computing

Contact Information
-------------------
Any questions or bugs should be reported to DPDK community and to the ENIC PMD
maintainers:

- John Daley <johndale@cisco.com>
- Nelson Escobar <neescoba@cisco.com>
