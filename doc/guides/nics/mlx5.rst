..  BSD LICENSE
    Copyright 2015 6WIND S.A.

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

The MLX5 poll mode driver library (**librte_pmd_mlx5**) provides support for
**Mellanox ConnectX-4** and **Mellanox ConnectX-4 Lx** families of
10/25/40/50/100 Gb/s adapters as well as their virtual functions (VF) in
SR-IOV context.

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

Enabling librte_pmd_mlx5 causes DPDK applications to be linked against
libibverbs.

Features
--------

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
- Flow director (RTE_FDIR_MODE_PERFECT and RTE_FDIR_MODE_PERFECT_MAC_VLAN).
- Secondary process TX is supported.
- KVM and VMware ESX SR-IOV modes are supported.

Limitations
-----------

- Inner RSS for VXLAN frames is not supported yet.
- Port statistics through software counters only.
- Hardware checksum offloads for VXLAN inner header are not supported yet.
- Secondary process RX is not supported.

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
  allows to save PCI bandwidth and improve performance at the cost of a
  slightly higher CPU usage.  Enabled by default.

  Supported on:

  - x86_64 with ConnectX4 and ConnectX4 LX
  - Power8 with ConnectX4 LX

- ``txq_inline`` parameter [int]

  Amount of data to be inlined during TX operations. Improves latency.
  Can improve PPS performance when PCI back pressure is detected and may be
  useful for scenarios involving heavy traffic on many queues.

  It is not enabled by default (set to 0) since the additional software
  logic necessary to handle this mode can lower performance when back
  pressure is not expected.

- ``txqs_min_inline`` parameter [int]

  Enable inline send only when the number of TX queues is greater or equal
  to this value.

  This option should be used in combination with ``txq_inline`` above.

- ``txq_mpw_en`` parameter [int]

  A nonzero value enables multi-packet send. This feature allows the TX
  burst function to pack up to five packets in two descriptors in order to
  save PCI bandwidth and improve performance at the cost of a slightly
  higher CPU usage.

  It is currently only supported on the ConnectX-4 Lx family of adapters.
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

  Low-level user space driver library for Mellanox ConnectX-4 devices,
  it is automatically loaded by libibverbs.

  This library basically implements send/receive calls to the hardware
  queues.

- **Kernel modules** (mlnx-ofed-kernel)

  They provide the kernel-side Verbs API and low level device drivers that
  manage actual hardware initialization and resources sharing with user
  space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to
  their devices:

  - mlx5_core: hardware driver managing Mellanox ConnectX-4 devices and
    related Ethernet kernel network devices.
  - mlx5_ib: InifiniBand device driver.
  - ib_uverbs: user space driver for Verbs (entry point for libibverbs).

- **Firmware update**

  Mellanox OFED releases include firmware updates for ConnectX-4 adapters.

  Because each release provides new features, these updates must be applied to
  match the kernel modules and libraries they come with.

.. note::

   Both libraries are BSD and GPL licensed. Linux kernel modules are GPL
   licensed.

Currently supported by DPDK:

- Mellanox OFED **3.3-1.0.0.0** and **3.3-2.0.0.0**.

- Minimum firmware version:

  - ConnectX-4: **12.16.1006**
  - ConnectX-4 Lx: **14.16.1006**

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

This section demonstrates how to launch **testpmd** with Mellanox ConnectX-4
devices managed by librte_pmd_mlx5.

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

      testpmd -c 0xff00 -n 4 -w 05:00.0 -w 05:00.1 -w 06:00.0 -w 06:00.1 -- --rxq=2 --txq=2 -i

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
