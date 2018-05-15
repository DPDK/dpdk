..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2015 6WIND S.A.
    Copyright 2015 Mellanox Technologies, Ltd

MLX5 poll mode driver
=====================

The MLX5 poll mode driver library (**librte_pmd_mlx5**) provides support
for **Mellanox ConnectX-4**, **Mellanox ConnectX-4 Lx** , **Mellanox
ConnectX-5** and **Mellanox Bluefield** families of 10/25/40/50/100 Gb/s
adapters as well as their virtual functions (VF) in SR-IOV context.

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
- Multiple process.
- KVM and VMware ESX SR-IOV modes are supported.
- RSS hash result is supported.
- Hardware TSO for generic IP or UDP tunnel, including VXLAN and GRE.
- Hardware checksum Tx offload for generic IP or UDP tunnel, including VXLAN and GRE.
- RX interrupts.
- Statistics query including Basic, Extended and per queue.
- Rx HW timestamp.
- Tunnel types: VXLAN, L3 VXLAN, VXLAN-GPE, GRE, MPLSoGRE, MPLSoUDP.
- Tunnel HW offloads: packet type, inner/outer RSS, IP and UDP checksum verification.

Limitations
-----------

- For secondary process:

  - Forked secondary process not supported.
  - All mempools must be initialized before rte_eth_dev_start().

- Flow pattern without any specific vlan will match for vlan packets as well:

  When VLAN spec is not specified in the pattern, the matching rule will be created with VLAN as a wild card.
  Meaning, the flow rule::

        flow create 0 ingress pattern eth / vlan vid is 3 / ipv4 / end ...

  Will only match vlan packets with vid=3. and the flow rules::

        flow create 0 ingress pattern eth / ipv4 / end ...

  Or::

        flow create 0 ingress pattern eth / vlan / ipv4 / end ...

  Will match any ipv4 packet (VLAN included).

- A multi segment packet must have less than 6 segments in case the Tx burst function
  is set to multi-packet send or Enhanced multi-packet send. Otherwise it must have
  less than 50 segments.

- Count action for RTE flow is **only supported in Mellanox OFED**.

- Flows with a VXLAN Network Identifier equal (or ends to be equal)
  to 0 are not supported.

- VXLAN TSO and checksum offloads are not supported on VM.

- L3 VXLAN and VXLAN-GPE tunnels cannot be supported together with MPLSoGRE and MPLSoUDP.

- VF: flow rules created on VF devices can only match traffic targeted at the
  configured MAC addresses (see ``rte_eth_dev_mac_addr_add()``).

.. note::

   MAC addresses not already present in the bridge table of the associated
   kernel network device will be added and cleaned up by the PMD when closing
   the device. In case of ungraceful program termination, some entries may
   remain present and should be removed manually by other means.

- When Multi-Packet Rx queue is configured (``mprq_en``), a Rx packet can be
  externally attached to a user-provided mbuf with having EXT_ATTACHED_MBUF in
  ol_flags. As the mempool for the external buffer is managed by PMD, all the
  Rx mbufs must be freed before the device is closed. Otherwise, the mempool of
  the external buffers will be freed by PMD and the application which still
  holds the external buffers may be corrupted.

Statistics
----------

MLX5 supports various of methods to report statistics:

Port statistics can be queried using ``rte_eth_stats_get()``. The port statistics are through SW only and counts the number of packets received or sent successfully by the PMD.

Extended statistics can be queried using ``rte_eth_xstats_get()``. The extended statistics expose a wider set of counters counted by the device. The extended port statistics counts the number of packets received or sent successfully by the port. As Mellanox NICs are using the :ref:`Bifurcated Linux Driver <linux_gsg_linux_drivers>` those counters counts also packet received or sent by the Linux kernel. The counters with ``_phy`` suffix counts the total events on the physical port, therefore not valid for VF.

Finally per-flow statistics can by queried using ``rte_flow_query`` when attaching a count action for specific flow. The flow counter counts the number of packets received successfully by the port and match the specific flow.

Configuration
-------------

Compilation options
~~~~~~~~~~~~~~~~~~~

These options can be modified in the ``.config`` file.

- ``CONFIG_RTE_LIBRTE_MLX5_PMD`` (default **n**)

  Toggle compilation of librte_pmd_mlx5 itself.

- ``CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS`` (default **n**)

  Build PMD with additional code to make it loadable without hard
  dependencies on **libibverbs** nor **libmlx5**, which may not be installed
  on the target system.

  In this mode, their presence is still required for it to run properly,
  however their absence won't prevent a DPDK application from starting (with
  ``CONFIG_RTE_BUILD_SHARED_LIB`` disabled) and they won't show up as
  missing with ``ldd(1)``.

  It works by moving these dependencies to a purpose-built rdma-core "glue"
  plug-in which must either be installed in a directory whose name is based
  on ``CONFIG_RTE_EAL_PMD_PATH`` suffixed with ``-glue`` if set, or in a
  standard location for the dynamic linker (e.g. ``/lib``) if left to the
  default empty string (``""``).

  This option has no performance impact.

- ``CONFIG_RTE_LIBRTE_MLX5_DEBUG`` (default **n**)

  Toggle debugging code and stricter compilation flags. Enabling this option
  adds additional run-time checks and debugging messages at the cost of
  lower performance.

Environment variables
~~~~~~~~~~~~~~~~~~~~~

- ``MLX5_GLUE_PATH``

  A list of directories in which to search for the rdma-core "glue" plug-in,
  separated by colons or semi-colons.

  Only matters when compiled with ``CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS``
  enabled and most useful when ``CONFIG_RTE_EAL_PMD_PATH`` is also set,
  since ``LD_LIBRARY_PATH`` has no effect in this case.

- ``MLX5_PMD_ENABLE_PADDING``

  Enables HW packet padding in PCI bus transactions.

  When packet size is cache aligned and CRC stripping is enabled, 4 fewer
  bytes are written to the PCI bus. Enabling padding makes such packets
  aligned again.

  In cases where PCI bandwidth is the bottleneck, padding can improve
  performance by 10%.

  This is disabled by default since this can also decrease performance for
  unaligned packet sizes.

- ``MLX5_SHUT_UP_BF``

  Configures HW Tx doorbell register as IO-mapped.

  By default, the HW Tx doorbell is configured as a write-combining register.
  The register would be flushed to HW usually when the write-combining buffer
  becomes full, but it depends on CPU design.

  Except for vectorized Tx burst routines, a write memory barrier is enforced
  after updating the register so that the update can be immediately visible to
  HW.

  When vectorized Tx burst is called, the barrier is set only if the burst size
  is not aligned to MLX5_VPMD_TX_MAX_BURST. However, setting this environmental
  variable will bring better latency even though the maximum throughput can
  slightly decline.

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

  - x86_64 with ConnectX-4, ConnectX-4 LX, ConnectX-5 and Bluefield.
  - POWER8 and ARMv8 with ConnectX-4 LX, ConnectX-5 and Bluefield.

- ``mprq_en`` parameter [int]

  A nonzero value enables configuring Multi-Packet Rx queues. Rx queue is
  configured as Multi-Packet RQ if the total number of Rx queues is
  ``rxqs_min_mprq`` or more and Rx scatter isn't configured. Disabled by
  default.

  Multi-Packet Rx Queue (MPRQ a.k.a Striding RQ) can further save PCIe bandwidth
  by posting a single large buffer for multiple packets. Instead of posting a
  buffers per a packet, one large buffer is posted in order to receive multiple
  packets on the buffer. A MPRQ buffer consists of multiple fixed-size strides
  and each stride receives one packet. MPRQ can improve throughput for
  small-packet tarffic.

  When MPRQ is enabled, max_rx_pkt_len can be larger than the size of
  user-provided mbuf even if DEV_RX_OFFLOAD_SCATTER isn't enabled. PMD will
  configure large stride size enough to accommodate max_rx_pkt_len as long as
  device allows. Note that this can waste system memory compared to enabling Rx
  scatter and multi-segment packet.

- ``mprq_log_stride_num`` parameter [int]

  Log 2 of the number of strides for Multi-Packet Rx queue. Configuring more
  strides can reduce PCIe tarffic further. If configured value is not in the
  range of device capability, the default value will be set with a warning
  message. The default value is 4 which is 16 strides per a buffer, valid only
  if ``mprq_en`` is set.

  The size of Rx queue should be bigger than the number of strides.

- ``mprq_max_memcpy_len`` parameter [int]

  The maximum length of packet to memcpy in case of Multi-Packet Rx queue. Rx
  packet is mem-copied to a user-provided mbuf if the size of Rx packet is less
  than or equal to this parameter. Otherwise, PMD will attach the Rx packet to
  the mbuf by external buffer attachment - ``rte_pktmbuf_attach_extbuf()``.
  A mempool for external buffers will be allocated and managed by PMD. If Rx
  packet is externally attached, ol_flags field of the mbuf will have
  EXT_ATTACHED_MBUF and this flag must be preserved. ``RTE_MBUF_HAS_EXTBUF()``
  checks the flag. The default value is 128, valid only if ``mprq_en`` is set.

- ``rxqs_min_mprq`` parameter [int]

  Configure Rx queues as Multi-Packet RQ if the total number of Rx queues is
  greater or equal to this value. The default value is 12, valid only if
  ``mprq_en`` is set.

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

  On ConnectX-4, ConnectX-4 LX, ConnectX-5 and Bluefield without
  Enhanced MPW:

        - Disabled by default.
        - In case ``txq_inline`` is set recommendation is 4.

  On ConnectX-5 and Bluefield with Enhanced MPW:

        - Set to 8 by default.

- ``txq_mpw_en`` parameter [int]

  A nonzero value enables multi-packet send (MPS) for ConnectX-4 Lx and
  enhanced multi-packet send (Enhanced MPS) for ConnectX-5 and Bluefield.
  MPS allows the TX burst function to pack up multiple packets in a
  single descriptor session in order to save PCI bandwidth and improve
  performance at the cost of a slightly higher CPU usage. When
  ``txq_inline`` is set along with ``txq_mpw_en``, TX burst function tries
  to copy entire packet data on to TX descriptor instead of including
  pointer of packet only if there is enough room remained in the
  descriptor. ``txq_inline`` sets per-descriptor space for either pointers
  or inlined packets. In addition, Enhanced MPS supports hybrid mode -
  mixing inlined packets and pointers in the same descriptor.

  This option cannot be used with certain offloads such as ``DEV_TX_OFFLOAD_TCP_TSO,
  DEV_TX_OFFLOAD_VXLAN_TNL_TSO, DEV_TX_OFFLOAD_GRE_TNL_TSO, DEV_TX_OFFLOAD_VLAN_INSERT``.
  When those offloads are requested the MPS send function will not be used.

  It is currently only supported on the ConnectX-4 Lx, ConnectX-5 and Bluefield
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

- ``tx_vec_en`` parameter [int]

  A nonzero value enables Tx vector on ConnectX-5 and Bluefield NICs if the number of
  global Tx queues on the port is lesser than MLX5_VPMD_MIN_TXQS.

  This option cannot be used with certain offloads such as ``DEV_TX_OFFLOAD_TCP_TSO,
  DEV_TX_OFFLOAD_VXLAN_TNL_TSO, DEV_TX_OFFLOAD_GRE_TNL_TSO, DEV_TX_OFFLOAD_VLAN_INSERT``.
  When those offloads are requested the MPS send function will not be used.

  Enabled by default on ConnectX-5 and Bluefield.

- ``rx_vec_en`` parameter [int]

  A nonzero value enables Rx vector if the port is not configured in
  multi-segment otherwise this parameter is ignored.

  Enabled by default.

- ``vf_nl_en`` parameter [int]

  A nonzero value enables Netlink requests from the VF to add/remove MAC
  addresses or/and enable/disable promiscuous/all multicast on the Netdevice.
  Otherwise the relevant configuration must be run with Linux iproute2 tools.
  This is a prerequisite to receive this kind of traffic.

  Enabled by default, valid only on VF devices ignored otherwise.

- ``l3_vxlan_en`` parameter [int]

  A nonzero value allows L3 VXLAN and VXLAN-GPE flow creation. To enable
  L3 VXLAN or VXLAN-GPE, users has to configure firmware and enable this
  parameter. This is a prerequisite to receive this kind of traffic.

  Disabled by default.

Firmware configuration
~~~~~~~~~~~~~~~~~~~~~~

- L3 VXLAN and VXLAN-GPE destination UDP port

   .. code-block:: console

     mlxconfig -d <mst device> set IP_OVER_VXLAN_EN=1
     mlxconfig -d <mst device> set IP_OVER_VXLAN_PORT=<udp dport>

  Verify configurations are set:

   .. code-block:: console

     mlxconfig -d <mst device> query | grep IP_OVER_VXLAN
     IP_OVER_VXLAN_EN                    True(1)
     IP_OVER_VXLAN_PORT                  <udp dport>

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

  Low-level user space driver library for Mellanox
  ConnectX-4/ConnectX-5/Bluefield devices, it is automatically loaded
  by libibverbs.

  This library basically implements send/receive calls to the hardware
  queues.

- **Kernel modules**

  They provide the kernel-side Verbs API and low level device drivers that
  manage actual hardware initialization and resources sharing with user
  space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to
  their devices:

  - mlx5_core: hardware driver managing Mellanox
    ConnectX-4/ConnectX-5/Bluefield devices and related Ethernet kernel
    network devices.
  - mlx5_ib: InifiniBand device driver.
  - ib_uverbs: user space driver for Verbs (entry point for libibverbs).

- **Firmware update**

  Mellanox OFED releases include firmware updates for
  ConnectX-4/ConnectX-5/Bluefield adapters.

  Because each release provides new features, these updates must be applied to
  match the kernel modules and libraries they come with.

.. note::

   Both libraries are BSD and GPL licensed. Linux kernel modules are GPL
   licensed.

Installation
~~~~~~~~~~~~

Either RDMA Core library with a recent enough Linux kernel release
(recommended) or Mellanox OFED, which provides compatibility with older
releases.

RMDA Core with Linux Kernel
^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Minimal kernel version : v4.14 or the most recent 4.14-rc (see `Linux installation documentation`_)
- Minimal rdma-core version: v15+ commit 0c5f5765213a ("Merge pull request #227 from yishaih/tm")
  (see `RDMA Core installation documentation`_)

.. _`Linux installation documentation`: https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/plain/Documentation/admin-guide/README.rst
.. _`RDMA Core installation documentation`: https://raw.githubusercontent.com/linux-rdma/rdma-core/master/README.md

Mellanox OFED
^^^^^^^^^^^^^

- Mellanox OFED version: **4.2, 4.3**.
- firmware version:

  - ConnectX-4: **12.21.1000** and above.
  - ConnectX-4 Lx: **14.21.1000** and above.
  - ConnectX-5: **16.21.1000** and above.
  - ConnectX-5 Ex: **16.21.1000** and above.
  - Bluefield: **18.99.3950** and above.

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

Quick Start Guide on OFED
-------------------------

1. Download latest Mellanox OFED. For more info check the  `prerequisites`_.


2. Install the required libraries and kernel modules either by installing
   only the required set, or by installing the entire Mellanox OFED:

   .. code-block:: console

        ./mlnxofedinstall --upstream-libs --dpdk

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

7. To minimize overhead of searching Memory Regions:

   - '--socket-mem' is recommended to pin memory by predictable amount.
   - Configure per-lcore cache when creating Mempools for packet buffer.
   - Refrain from dynamically allocating/freeing memory in run-time.

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
ConnectX-4/ConnectX-5/Bluefield devices managed by librte_pmd_mlx5.

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
