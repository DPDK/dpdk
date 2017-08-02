..  BSD LICENSE
    Copyright 2012-2015 6WIND S.A.
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

MLX4 poll mode driver library
=============================

The MLX4 poll mode driver library (**librte_pmd_mlx4**) implements support
for **Mellanox ConnectX-3** and **Mellanox ConnectX-3 Pro** 10/40 Gbps adapters
as well as their virtual functions (VF) in SR-IOV context.

Information and documentation about this family of adapters can be found on
the `Mellanox website <http://www.mellanox.com>`_. Help is also provided by
the `Mellanox community <http://community.mellanox.com/welcome>`_.

There is also a `section dedicated to this poll mode driver
<http://www.mellanox.com/page/products_dyn?product_family=209&mtag=pmd_for_dpdk>`_.

.. note::

   Due to external dependencies, this driver is disabled by default. It must
   be enabled manually by setting ``CONFIG_RTE_LIBRTE_MLX4_PMD=y`` and
   recompiling DPDK.

Implementation details
----------------------

Most Mellanox ConnectX-3 devices provide two ports but expose a single PCI
bus address, thus unlike most drivers, librte_pmd_mlx4 registers itself as a
PCI driver that allocates one Ethernet device per detected port.

For this reason, one cannot white/blacklist a single port without also
white/blacklisting the others on the same device.

Besides its dependency on libibverbs (that implies libmlx4 and associated
kernel support), librte_pmd_mlx4 relies heavily on system calls for control
operations such as querying/updating the MTU and flow control parameters.

For security reasons and robustness, this driver only deals with virtual
memory addresses. The way resources allocations are handled by the kernel
combined with hardware specifications that allow it to handle virtual memory
addresses directly ensure that DPDK applications cannot access random
physical memory (or memory that does not belong to the current process).

This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets as
long as they share the same MAC address.

Compiling librte_pmd_mlx4 causes DPDK to be linked against libibverbs.

Features
--------

- Multi arch support: x86_64 and POWER8.
- RSS, also known as RCA, is supported. In this mode the number of
  configured RX queues must be a power of two.
- VLAN filtering is supported.
- Link state information is provided.
- Promiscuous mode is supported.
- All multicast mode is supported.
- Multiple MAC addresses (unicast, multicast) can be configured.
- Scattered packets are supported for TX and RX.
- Inner L3/L4 (IP, TCP and UDP) TX/RX checksum offloading and validation.
- Outer L3 (IP) TX/RX checksum offloading and validation for VXLAN frames.
- Secondary process TX is supported.
- RX interrupts.

Limitations
-----------

- RSS hash key cannot be modified.
- RSS RETA cannot be configured
- RSS always includes L3 (IPv4/IPv6) and L4 (UDP/TCP). They cannot be
  dissociated.
- Hardware counters are not implemented (they are software counters).
- Secondary process RX is not supported.

Configuration
-------------

Compilation options
~~~~~~~~~~~~~~~~~~~

These options can be modified in the ``.config`` file.

- ``CONFIG_RTE_LIBRTE_MLX4_PMD`` (default **n**)

  Toggle compilation of librte_pmd_mlx4 itself.

- ``CONFIG_RTE_LIBRTE_MLX4_DEBUG`` (default **n**)

  Toggle debugging code and stricter compilation flags. Enabling this option
  adds additional run-time checks and debugging messages at the cost of
  lower performance.

- ``CONFIG_RTE_LIBRTE_MLX4_DEBUG_BROKEN_VERBS`` (default **n**)

  Mellanox OFED versions earlier than 4.2 may return false errors from
  Verbs object destruction APIs after the device is plugged out.
  Enabling this option replaces assertion checks that cause the program
  to abort with harmless debugging messages as a workaround.
  Relevant only when CONFIG_RTE_LIBRTE_MLX4_DEBUG is enabled.

- ``CONFIG_RTE_LIBRTE_MLX4_SGE_WR_N`` (default **4**)

  Number of scatter/gather elements (SGEs) per work request (WR). Lowering
  this number improves performance but also limits the ability to receive
  scattered packets (packets that do not fit a single mbuf). The default
  value is a safe tradeoff.

- ``CONFIG_RTE_LIBRTE_MLX4_MAX_INLINE`` (default **0**)

  Amount of data to be inlined during TX operations. Improves latency but
  lowers throughput.

- ``CONFIG_RTE_LIBRTE_MLX4_TX_MP_CACHE`` (default **8**)

  Maximum number of cached memory pools (MPs) per TX queue. Each MP from
  which buffers are to be transmitted must be associated to memory regions
  (MRs). This is a slow operation that must be cached.

  This value is always 1 for RX queues since they use a single MP.

- ``CONFIG_RTE_LIBRTE_MLX4_SOFT_COUNTERS`` (default **1**)

  Toggle software counters. No counters are available if this option is
  disabled since hardware counters are not supported.

Environment variables
~~~~~~~~~~~~~~~~~~~~~

- ``MLX4_INLINE_RECV_SIZE``

  A nonzero value enables inline receive for packets up to that size. May
  significantly improve performance in some cases but lower it in
  others. Requires careful testing.

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- The only constraint when RSS mode is requested is to make sure the number
  of RX queues is a power of two. This is a hardware requirement.

- librte_pmd_mlx4 brings kernel network interfaces up during initialization
  because it is affected by their state. Forcing them down prevents packets
  reception.

- **ethtool** operations on related kernel interfaces also affect the PMD.

- ``port`` parameter [int]

  This parameter provides a physical port to probe and can be specified multiple
  times for additional ports. All ports are probed by default if left
  unspecified.

Kernel module parameters
~~~~~~~~~~~~~~~~~~~~~~~~

The **mlx4_core** kernel module has several parameters that affect the
behavior and/or the performance of librte_pmd_mlx4. Some of them are described
below.

- **num_vfs** (integer or triplet, optionally prefixed by device address
  strings)

  Create the given number of VFs on the specified devices.

- **log_num_mgm_entry_size** (integer)

  Device-managed flow steering (DMFS) is required by DPDK applications. It is
  enabled by using a negative value, the last four bits of which have a
  special meaning.

  - **-1**: force device-managed flow steering (DMFS).
  - **-7**: configure optimized steering mode to improve performance with the
    following limitation: VLAN filtering is not supported with this mode.
    This is the recommended mode in case VLAN filter is not needed.

Prerequisites
-------------

This driver relies on external libraries and kernel drivers for resources
allocations and initialization. The following dependencies are not part of
DPDK and must be installed separately:

- **libibverbs**

  User space verbs framework used by librte_pmd_mlx4. This library provides
  a generic interface between the kernel and low-level user space drivers
  such as libmlx4.

  It allows slow and privileged operations (context initialization, hardware
  resources allocations) to be managed by the kernel and fast operations to
  never leave user space.

- **libmlx4**

  Low-level user space driver library for Mellanox ConnectX-3 devices,
  it is automatically loaded by libibverbs.

  This library basically implements send/receive calls to the hardware
  queues.

- **Kernel modules** (mlnx-ofed-kernel)

  They provide the kernel-side verbs API and low level device drivers that
  manage actual hardware initialization and resources sharing with user
  space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to
  their devices:

  - mlx4_core: hardware driver managing Mellanox ConnectX-3 devices.
  - mlx4_en: Ethernet device driver that provides kernel network interfaces.
  - mlx4_ib: InifiniBand device driver.
  - ib_uverbs: user space driver for verbs (entry point for libibverbs).

- **Firmware update**

  Mellanox OFED releases include firmware updates for ConnectX-3 adapters.

  Because each release provides new features, these updates must be applied to
  match the kernel modules and libraries they come with.

.. note::

   Both libraries are BSD and GPL licensed. Linux kernel modules are GPL
   licensed.

Currently supported by DPDK:

- Mellanox OFED **4.1**.
- Firmware version **2.36.5000** and above.

Getting Mellanox OFED
~~~~~~~~~~~~~~~~~~~~~

While these libraries and kernel modules are available on OpenFabrics
Alliance's `website <https://www.openfabrics.org/>`_ and provided by package
managers on most distributions, this PMD requires Ethernet extensions that
may not be supported at the moment (this is a work in progress).

`Mellanox OFED
<http://www.mellanox.com/page/products_dyn?product_family=26&mtag=linux_sw_drivers>`_
includes the necessary support and should be used in the meantime. For DPDK,
only libibverbs, libmlx4, mlnx-ofed-kernel packages and firmware updates are
required from that distribution.

.. note::

   Several versions of Mellanox OFED are available. Installing the version
   this DPDK release was developed and tested against is strongly
   recommended. Please check the `prerequisites`_.

Supported NICs
--------------

* Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2*40G)

Quick Start Guide
-----------------

1. Download latest Mellanox OFED. For more info check the  `prerequisites`_.

2. Install the required libraries and kernel modules either by installing
   only the required set, or by installing the entire Mellanox OFED:

   For bare metal use:

   .. code-block:: console

        ./mlnxofedinstall

   For SR-IOV hypervisors use:

   .. code-block:: console

        ./mlnxofedinstall --enable-sriov -hypervisor

   For SR-IOV virtual machine use:

   .. code-block:: console

        ./mlnxofedinstall --guest

3. Verify the firmware is the correct one:

   .. code-block:: console

        ibv_devinfo

4. Set all ports links to Ethernet, follow instructions on the screen:

   .. code-block:: console

        connectx_port_config

   Or in the manual way:

   .. code-block:: console

        PCI=<NIC PCI address>
        echo eth > "/sys/bus/pci/devices/$PCI/mlx4_port0"
        echo eth > "/sys/bus/pci/devices/$PCI/mlx4_port1"

5. In case of bare metal or hypervisor, configure optimized steering mode
   by adding the following line to ``/etc/modprobe.d/mlx4_core.conf``:

   .. code-block:: console

        options mlx4_core log_num_mgm_entry_size=-7

   .. note::

        If VLAN filtering is used, set log_num_mgm_entry_size=-1.
        Performance degradation can occur on this case.

6. Restart the driver:

   .. code-block:: console

        /etc/init.d/openibd restart

   or:

   .. code-block:: console

        service openibd restart

7. Compile DPDK and you are ready to go. See instructions on
   :ref:`Development Kit Build System <Development_Kit_Build_System>`

Performance tuning
------------------

1. Verify the optimized steering mode is configured:

  .. code-block:: console

        cat /sys/module/mlx4_core/parameters/log_num_mgm_entry_size

2. Use environment variable MLX4_INLINE_RECV_SIZE=64 to get maximum
   performance for 64B messages.

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

Usage example
-------------

This section demonstrates how to launch **testpmd** with Mellanox ConnectX-3
devices managed by librte_pmd_mlx4.

#. Load the kernel modules:

   .. code-block:: console

      modprobe -a ib_uverbs mlx4_en mlx4_core mlx4_ib

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

      eth2
      eth3
      eth4
      eth5

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

      -w 0000:83:00.0
      -w 0000:83:00.0
      -w 0000:84:00.0
      -w 0000:84:00.0

   .. note::

      There are only two distinct PCI bus addresses because the Mellanox
      ConnectX-3 adapters installed on this system are dual port.

#. Request huge pages:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages/nr_hugepages

#. Start testpmd with basic parameters:

   .. code-block:: console

      testpmd -l 8-15 -n 4 -w 0000:83:00.0 -w 0000:84:00.0 -- --rxq=2 --txq=2 -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:83:00.0 on NUMA socket 1
      EAL:   probe driver: 15b3:1007 librte_pmd_mlx4
      PMD: librte_pmd_mlx4: PCI information matches, using device "mlx4_0" (VF: false)
      PMD: librte_pmd_mlx4: 2 port(s) detected
      PMD: librte_pmd_mlx4: port 1 MAC address is 00:02:c9:b5:b7:50
      PMD: librte_pmd_mlx4: port 2 MAC address is 00:02:c9:b5:b7:51
      EAL: PCI device 0000:84:00.0 on NUMA socket 1
      EAL:   probe driver: 15b3:1007 librte_pmd_mlx4
      PMD: librte_pmd_mlx4: PCI information matches, using device "mlx4_1" (VF: false)
      PMD: librte_pmd_mlx4: 2 port(s) detected
      PMD: librte_pmd_mlx4: port 1 MAC address is 00:02:c9:b5:ba:b0
      PMD: librte_pmd_mlx4: port 2 MAC address is 00:02:c9:b5:ba:b1
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      PMD: librte_pmd_mlx4: 0x867d60: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx4: 0x867d60: RX queues number update: 0 -> 2
      Port 0: 00:02:C9:B5:B7:50
      Configuring Port 1 (socket 0)
      PMD: librte_pmd_mlx4: 0x867da0: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx4: 0x867da0: RX queues number update: 0 -> 2
      Port 1: 00:02:C9:B5:B7:51
      Configuring Port 2 (socket 0)
      PMD: librte_pmd_mlx4: 0x867de0: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx4: 0x867de0: RX queues number update: 0 -> 2
      Port 2: 00:02:C9:B5:BA:B0
      Configuring Port 3 (socket 0)
      PMD: librte_pmd_mlx4: 0x867e20: TX queues number update: 0 -> 2
      PMD: librte_pmd_mlx4: 0x867e20: RX queues number update: 0 -> 2
      Port 3: 00:02:C9:B5:BA:B1
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 40000 Mbps - full-duplex
      Port 2 Link Up - speed 10000 Mbps - full-duplex
      Port 3 Link Up - speed 40000 Mbps - full-duplex
      Done
      testpmd>
