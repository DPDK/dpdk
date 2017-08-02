..  BSD LICENSE
    Copyright 2015 6WIND S.A.
    Copyright 2015 Mellanox

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of 6WIND S.A. nor the names of its
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

MLX5 poll mode driver
=====================

The MLX5 poll mode driver library (**librte_pmd_mlx5**) provides support
for **Mellanox ConnectX-4**, **Mellanox ConnectX-4 Lx** and **Mellanox
ConnectX-5** families of 10/25/40/50/100 Gb/s adapters as well as their
virtual functions (VF) in SR-IOV context.

Information and documentation about these adapters can be found on the
`Mellanox website <http://www.mellanox.com>`__. Help is also provided by the
`Mellanox community <http://community.mellanox.com/welcome>`__.

There is also a `section dedicated to this poll mode driver
<http://www.mellanox.com/page/products_dyn?product_family=209&mtag=pmd_for_dpdk>`__.

.. note::

   Due to external dependencies, this driver is disabled by default. It must
   be enabled manually by setting ``CONFIG_RTE_LIBRTE_MLX5_PMD=y`` and
   recompiling DPDK.

Implementation details
----------------------

Besides its dependency on libibverbs (that implies libmlx5 and associated
kernel support), librte_pmd_mlx5 relies heavily on system calls for control
operations such as querying/updating the MTU and flow control parameters.

For security reasons and robustness, this driver only deals with virtual
memory addresses. The way resources allocations are handled by the kernel
combined with hardware specifications that allow it to handle virtual memory
addresses directly ensure that DPDK applications cannot access random
physical memory (or memory that does not belong to the current process).

This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets as
long as they share the same MAC address.
This means legacy linux control tools (for example: ethtool, ifconfig and
more) can operate on the same network interfaces that owned by the DPDK
application.

Enabling librte_pmd_mlx5 causes DPDK applications to be linked against
libibverbs.

Features
--------

- Multi arch support: x86_64, POWER8, ARMv8.
- Multiple TX and RX queues.
- Support for scattered TX and RX frames.
- IPv4, IPv6, TCPv4, TCPv6, UDPv4 and UDPv6 RSS on any number of queues.
- Several RSS hash keys, one for each flow type.
- Configurable RETA table.
- Support for multiple MAC addresses.
- VLAN filtering.
- RX VLAN stripping.
- TX VLAN insertion.
- RX CRC stripping configuration.
- Promiscuous mode.
- Multicast promiscuous mode.
- Hardware checksum offloads.
- Flow director (RTE_FDIR_MODE_PERFECT, RTE_FDIR_MODE_PERFECT_MAC_VLAN and
  RTE_ETH_FDIR_REJECT).
- Flow API.
- Secondary process TX is supported.
- KVM and VMware ESX SR-IOV modes are supported.
- RSS hash result is supported.
- Hardware TSO.
- Hardware checksum TX offload for VXLAN and GRE.
- RX interrupts.
- Statistics query including Basic, Extended and per queue.

Limitations
-----------

- Inner RSS for VXLAN frames is not supported yet.
- Port statistics through software counters only.
- Hardware checksum RX offloads for VXLAN inner header are not supported yet.
- Secondary process RX is not supported.
- Flow pattern without any specific vlan will match for vlan packets as well:

  When VLAN spec is not specified in the pattern, the matching rule will be created with VLAN as a wild card.
  Meaning, the flow rule::

        flow create 0 ingress pattern eth / vlan vid is 3 / ipv4 / end ...

  Will only match vlan packets with vid=3. and the flow rules::

        flow create 0 ingress pattern eth / ipv4 / end ...

  Or::

        flow create 0 ingress pattern eth / vlan / ipv4 / end ...

  Will match any ipv4 packet (VLAN included).

Configuration
-------------

Compilation options
~~~~~~~~~~~~~~~~~~~

These options can be modified in the ``.config`` file.

- ``CONFIG_RTE_LIBRTE_MLX5_PMD`` (default **n**)

  Toggle compilation of librte_pmd_mlx5 itself.

- ``CONFIG_RTE_LIBRTE_MLX5_DEBUG`` (default **n**)

  Toggle debugging code and stricter compilation flags. Enabling this option
  adds additional run-time checks and debugging messages at the cost of
  lower performance.

- ``CONFIG_RTE_LIBRTE_MLX5_TX_MP_CACHE`` (default **8**)

  Maximum number of cached memory pools (MPs) per TX queue. Each MP from
  which buffers are to be transmitted must be associated to memory regions
  (MRs). This is a slow operation that must be cached.

  This value is always 1 for RX queues since they use a single MP.

Environment variables
~~~~~~~~~~~~~~~~~~~~~

- ``MLX5_PMD_ENABLE_PADDING``

  Enables HW packet padding in PCI bus transactions.

  When packet size is cache aligned and CRC stripping is enabled, 4 fewer
  bytes are written to the PCI bus. Enabling padding makes such packets
  aligned again.

  In cases where PCI bandwidth is the bottleneck, padding can improve
  performance by 10%.

  This is disabled by default since this can also decrease performance for
  unaligned packet sizes.

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- librte_pmd_mlx5 brings kernel network interfaces up during initialization
  because it is affected by their state. Forcing them down prevents packets
  reception.

- **ethtool** operations on related kernel interfaces also affect the PMD.

- ``rxq_cqe_comp_en`` parameter [int]

  A nonzero value enables the compression of CQE on RX side. This feature
  allows to save PCI bandwidth and improve performance. Enabled by default.

  Supported on:

  - x86_64 with ConnectX-4, ConnectX-4 LX and ConnectX-5.
  - POWER8 and ARMv8 with ConnectX-4 LX and ConnectX-5.

- ``txq_inline`` parameter [int]

  Amount of data to be inlined during TX operations. Improves latency.
  Can improve PPS performance when PCI back pressure is detected and may be
  useful for scenarios involving heavy traffic on many queues.

  Because additional software logic is necessary to handle this mode, this
  option should be used with care, as it can lower performance when back
  pressure is not expected.

- ``txqs_min_inline`` parameter [int]

  Enable inline send only when the number of TX queues is greater or equal
  to this value.

  This option should be used in combination with ``txq_inline`` above.

  On ConnectX-4, ConnectX-4 LX and ConnectX-5 without Enhanced MPW:

        - Disabled by default.
        - In case ``txq_inline`` is set recommendation is 4.

  On ConnectX-5 with Enhanced MPW:

        - Set to 8 by default.

- ``txq_mpw_en`` parameter [int]

  A nonzero value enables multi-packet send (MPS) for ConnectX-4 Lx and
  enhanced multi-packet send (Enhanced MPS) for ConnectX-5. MPS allows the
  TX burst function to pack up multiple packets in a single descriptor
  session in order to save PCI bandwidth and improve performance at the
  cost of a slightly higher CPU usage. When ``txq_inline`` is set along
  with ``txq_mpw_en``, TX burst function tries to copy entire packet data
  on to TX descriptor instead of including pointer of packet only if there
  is enough room remained in the descriptor. ``txq_inline`` sets
  per-descriptor space for either pointers or inlined packets. In addition,
  Enhanced MPS supports hybrid mode - mixing inlined packets and pointers
  in the same descriptor.

  This option cannot be used in conjunction with ``tso`` below. When ``tso``
  is set, ``txq_mpw_en`` is disabled.

  It is currently only supported on the ConnectX-4 Lx and ConnectX-5
  families of adapters. Enabled by default.

- ``txq_mpw_hdr_dseg_en`` parameter [int]

  A nonzero value enables including two pointers in the first block of TX
  descriptor. This can be used to lessen CPU load for memory copy.

  Effective only when Enhanced MPS is supported. Disabled by default.

- ``txq_max_inline_len`` parameter [int]

  Maximum size of packet to be inlined. This limits the size of packet to
  be inlined. If the size of a packet is larger than configured value, the
  packet isn't inlined even though there's enough space remained in the
  descriptor. Instead, the packet is included with pointer.

  Effective only when Enhanced MPS is supported. The default value is 256.

- ``tso`` parameter [int]

  A nonzero value enables hardware TSO.
  When hardware TSO is enabled, packets marked with TCP segmentation
  offload will be divided into segments by the hardware. Disabled by default.

- ``tx_vec_en`` parameter [int]

  A nonzero value enables Tx vector on ConnectX-5 only NIC if the number of
  global Tx queues on the port is lesser than MLX5_VPMD_MIN_TXQS.

  Enabled by default on ConnectX-5.

- ``rx_vec_en`` parameter [int]

  A nonzero value enables Rx vector if the port is not configured in
  multi-segment otherwise this parameter is ignored.

  Enabled by default.

Prerequisites
-------------

This driver relies on external libraries and kernel drivers for resources
allocations and initialization. The following dependencies are not part of
DPDK and must be installed separately:

- **libibverbs**

  User space Verbs framework used by librte_pmd_mlx5. This library provides
  a generic interface between the kernel and low-level user space drivers
  such as libmlx5.

  It allows slow and privileged operations (context initialization, hardware
  resources allocations) to be managed by the kernel and fast operations to
  never leave user space.

- **libmlx5**

  Low-level user space driver library for Mellanox ConnectX-4/ConnectX-5
  devices, it is automatically loaded by libibverbs.

  This library basically implements send/receive calls to the hardware
  queues.

- **Kernel modules** (mlnx-ofed-kernel)

  They provide the kernel-side Verbs API and low level device drivers that
  manage actual hardware initialization and resources sharing with user
  space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to
  their devices:

  - mlx5_core: hardware driver managing Mellanox ConnectX-4/ConnectX-5
    devices and related Ethernet kernel network devices.
  - mlx5_ib: InifiniBand device driver.
  - ib_uverbs: user space driver for Verbs (entry point for libibverbs).

- **Firmware update**

  Mellanox OFED releases include firmware updates for ConnectX-4/ConnectX-5
  adapters.

  Because each release provides new features, these updates must be applied to
  match the kernel modules and libraries they come with.

.. note::

   Both libraries are BSD and GPL licensed. Linux kernel modules are GPL
   licensed.

Currently supported by DPDK:

- Mellanox OFED version: **4.1**.
- firmware version:

  - ConnectX-4: **12.20.1010** and above.
  - ConnectX-4 Lx: **14.20.1010** and above.
  - ConnectX-5: **16.20.1010** and above.
  - ConnectX-5 Ex: **16.20.1010** and above.

Getting Mellanox OFED
~~~~~~~~~~~~~~~~~~~~~

While these libraries and kernel modules are available on OpenFabrics
Alliance's `website <https://www.openfabrics.org/>`__ and provided by package
managers on most distributions, this PMD requires Ethernet extensions that
may not be supported at the moment (this is a work in progress).

`Mellanox OFED
<http://www.mellanox.com/page/products_dyn?product_family=26&mtag=linux>`__
includes the necessary support and should be used in the meantime. For DPDK,
only libibverbs, libmlx5, mlnx-ofed-kernel packages and firmware updates are
required from that distribution.

.. note::

   Several versions of Mellanox OFED are available. Installing the version
   this DPDK release was developed and tested against is strongly
   recommended. Please check the `prerequisites`_.

Supported NICs
--------------

* Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)
* Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)
* Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)
* Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)
* Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT (1x40G)
* Mellanox(R) ConnectX(R)-4 40G MCX413A-BCAT (1x40G)
* Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)
* Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT (1x50G)
* Mellanox(R) ConnectX(R)-4 50G MCX413A-GCAT (1x50G)
* Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)
* Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT (2x50G)
* Mellanox(R) ConnectX(R)-4 50G MCX416A-BCAT (2x50G)
* Mellanox(R) ConnectX(R)-4 50G MCX416A-GCAT (2x50G)
* Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)
* Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)
* Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)
* Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)
* Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)
* Mellanox(R) ConnectX(R)-5 Ex EN 100G MCX516A-CDAT (2x100G)

Quick Start Guide
-----------------

1. Download latest Mellanox OFED. For more info check the  `prerequisites`_.


2. Install the required libraries and kernel modules either by installing
   only the required set, or by installing the entire Mellanox OFED:

   .. code-block:: console

        ./mlnxofedinstall

3. Verify the firmware is the correct one:

   .. code-block:: console

        ibv_devinfo

4. Verify all ports links are set to Ethernet:

   .. code-block:: console

        mlxconfig -d <mst device> query | grep LINK_TYPE
        LINK_TYPE_P1                        ETH(2)
        LINK_TYPE_P2                        ETH(2)

   Link types may have to be configured to Ethernet:

   .. code-block:: console

        mlxconfig -d <mst device> set LINK_TYPE_P1/2=1/2/3

        * LINK_TYPE_P1=<1|2|3> , 1=Infiniband 2=Ethernet 3=VPI(auto-sense)

   For hypervisors verify SR-IOV is enabled on the NIC:

   .. code-block:: console

        mlxconfig -d <mst device> query | grep SRIOV_EN
        SRIOV_EN                            True(1)

   If needed, set enable the set the relevant fields:

   .. code-block:: console

        mlxconfig -d <mst device> set SRIOV_EN=1 NUM_OF_VFS=16
        mlxfwreset -d <mst device> reset

5. Restart the driver:

   .. code-block:: console

        /etc/init.d/openibd restart

   or:

   .. code-block:: console

        service openibd restart

   If link type was changed, firmware must be reset as well:

   .. code-block:: console

        mlxfwreset -d <mst device> reset

   For hypervisors, after reset write the sysfs number of virtual functions
   needed for the PF.

   To dynamically instantiate a given number of virtual functions (VFs):

   .. code-block:: console

        echo [num_vfs] > /sys/class/infiniband/mlx5_0/device/sriov_numvfs

6. Compile DPDK and you are ready to go. See instructions on
   :ref:`Development Kit Build System <Development_Kit_Build_System>`

Performance tuning
------------------

1. Configure aggressive CQE Zipping for maximum performance:

  .. code-block:: console

        mlxconfig -d <mst device> s CQE_COMPRESSION=1

  To set it back to the default CQE Zipping mode use:

  .. code-block:: console

        mlxconfig -d <mst device> s CQE_COMPRESSION=0

2. In case of virtualization:

   - Make sure that hypervisor kernel is 3.16 or newer.
   - Configure boot with ``iommu=pt``.
   - Use 1G huge pages.
   - Make sure to allocate a VM on huge pages.
   - Make sure to set CPU pinning.

3. Use the CPU near local NUMA node to which the PCIe adapter is connected,
   for better performance. For VMs, verify that the right CPU
   and NUMA node are pinned according to the above. Run:

   .. code-block:: console

        lstopo-no-graphics

   to identify the NUMA node to which the PCIe adapter is connected.

4. If more than one adapter is used, and root complex capabilities allow
   to put both adapters on the same NUMA node without PCI bandwidth degradation,
   it is recommended to locate both adapters on the same NUMA node.
   This in order to forward packets from one to the other without
   NUMA performance penalty.

5. Disable pause frames:

   .. code-block:: console

        ethtool -A <netdev> rx off tx off

6. Verify IO non-posted prefetch is disabled by default. This can be checked
   via the BIOS configuration. Please contact you server provider for more
   information about the settings.

.. note::

        On some machines, depends on the machine integrator, it is beneficial
        to set the PCI max read request parameter to 1K. This can be
        done in the following way:

        To query the read request size use:

        .. code-block:: console

                setpci -s <NIC PCI address> 68.w

        If the output is different than 3XXX, set it by:

        .. code-block:: console

                setpci -s <NIC PCI address> 68.w=3XXX

        The XXX can be different on different systems. Make sure to configure
        according to the setpci output.

Notes for testpmd
-----------------

Compared to librte_pmd_mlx4 that implements a single RSS configuration per
port, librte_pmd_mlx5 supports per-protocol RSS configuration.

Since ``testpmd`` defaults to IP RSS mode and there is currently no
command-line parameter to enable additional protocols (UDP and TCP as well
as IP), the following commands must be entered from its CLI to get the same
behavior as librte_pmd_mlx4:

.. code-block:: console

   > port stop all
   > port config all rss all
   > port start all

Usage example
-------------

This section demonstrates how to launch **testpmd** with Mellanox
ConnectX-4/ConnectX-5 devices managed by librte_pmd_mlx5.

#. Load the kernel modules:

   .. code-block:: console

      modprobe -a ib_uverbs mlx5_core mlx5_ib

   Alternatively if MLNX_OFED is fully installed, the following script can
   be run:

   .. code-block:: console

      /etc/init.d/openibd restart

   .. note::

      User space I/O kernel modules (uio and igb_uio) are not used and do
      not have to be loaded.

#. Make sure Ethernet interfaces are in working order and linked to kernel
   verbs. Related sysfs entries should be present:

   .. code-block:: console

      ls -d /sys/class/net/*/device/infiniband_verbs/uverbs* | cut -d / -f 5

   Example output:

   .. code-block:: console

      eth30
      eth31
      eth32
      eth33

#. Optionally, retrieve their PCI bus addresses for whitelisting:

   .. code-block:: console

      {
          for intf in eth2 eth3 eth4 eth5;
          do
              (cd "/sys/class/net/${intf}/device/" && pwd -P);
          done;
      } |
      sed -n 's,.*/\(.*\),-w \1,p'

   Example output:

   .. code-block:: console

      -w 0000:05:00.1
      -w 0000:06:00.0
      -w 0000:06:00.1
      -w 0000:05:00.0

#. Request huge pages:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages/nr_hugepages

#. Start testpmd with basic parameters:

   .. code-block:: console

      testpmd -l 8-15 -n 4 -w 05:00.0 -w 05:00.1 -w 06:00.0 -w 06:00.1 -- --rxq=2 --txq=2 -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:05:00.0 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_pmd_mlx5
      PMD: librte_pmd_mlx5: PCI information matches, using device "mlx5_0" (VF: false)
      PMD: librte_pmd_mlx5: 1 port(s) detected
      PMD: librte_pmd_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:fe
      EAL: PCI device 0000:05:00.1 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_pmd_mlx5
      PMD: librte_pmd_mlx5: PCI information matches, using device "mlx5_1" (VF: false)
      PMD: librte_pmd_mlx5: 1 port(s) detected
      PMD: librte_pmd_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:ff
      EAL: PCI device 0000:06:00.0 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_pmd_mlx5
      PMD: librte_pmd_mlx5: PCI information matches, using device "mlx5_2" (VF: false)
      PMD: librte_pmd_mlx5: 1 port(s) detected
      PMD: librte_pmd_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:fa
      EAL: PCI device 0000:06:00.1 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_pmd_mlx5
      PMD: librte_pmd_mlx5: PCI information matches, using device "mlx5_3" (VF: false)
      PMD: librte_pmd_mlx5: 1 port(s) detected
      PMD: librte_pmd_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:fb
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      PMD: librte_pmd_mlx5: 0x8cba80: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx5: 0x8cba80: RX queues number update: 0 -> 2
      Port 0: E4:1D:2D:E7:0C:FE
      Configuring Port 1 (socket 0)
      PMD: librte_pmd_mlx5: 0x8ccac8: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx5: 0x8ccac8: RX queues number update: 0 -> 2
      Port 1: E4:1D:2D:E7:0C:FF
      Configuring Port 2 (socket 0)
      PMD: librte_pmd_mlx5: 0x8cdb10: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx5: 0x8cdb10: RX queues number update: 0 -> 2
      Port 2: E4:1D:2D:E7:0C:FA
      Configuring Port 3 (socket 0)
      PMD: librte_pmd_mlx5: 0x8ceb58: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx5: 0x8ceb58: RX queues number update: 0 -> 2
      Port 3: E4:1D:2D:E7:0C:FB
      Checking link statuses...
      Port 0 Link Up - speed 40000 Mbps - full-duplex
      Port 1 Link Up - speed 40000 Mbps - full-duplex
      Port 2 Link Up - speed 10000 Mbps - full-duplex
      Port 3 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>
