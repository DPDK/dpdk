..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2012 6WIND S.A.
    Copyright 2015 Mellanox Technologies, Ltd

NVIDIA MLX4 Ethernet Driver
===========================

.. note::

   NVIDIA acquired Mellanox Technologies in 2020.
   The DPDK documentation and code might still include instances
   of or references to Mellanox trademarks (like BlueField and ConnectX)
   that are now NVIDIA trademarks.

The MLX4 poll mode driver library (**librte_net_mlx4**) implements support
for **NVIDIA ConnectX-3** and **NVIDIA ConnectX-3 Pro** 10/40 Gbps adapters
as well as their virtual functions (VF) in SR-IOV context.

There is also a `section dedicated to this poll mode driver
<https://developer.nvidia.com/networking/dpdk>`_.


Implementation details
----------------------

Most NVIDIA ConnectX-3 devices provide two ports but expose a single PCI
bus address, thus unlike most drivers, librte_net_mlx4 registers itself as a
PCI driver that allocates one Ethernet device per detected port.

For this reason, one cannot block (or allow) a single port without also
blocking (or allowing) the others on the same device.

Besides its dependency on libibverbs (that implies libmlx4 and associated
kernel support), librte_net_mlx4 relies heavily on system calls for control
operations such as querying/updating the MTU and flow control parameters.

For security reasons and robustness, this driver only deals with virtual
memory addresses. The way resources allocations are handled by the kernel
combined with hardware specifications that allow it to handle virtual memory
addresses directly ensure that DPDK applications cannot access random
physical memory (or memory that does not belong to the current process).

This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets as
long as they share the same MAC address.

The :ref:`flow_isolated_mode` is supported.

Compiling librte_net_mlx4 causes DPDK to be linked against libibverbs.

Configuration
-------------

Compilation options
~~~~~~~~~~~~~~~~~~~

The ibverbs libraries can be linked with this PMD in a number of ways,
configured by the ``ibverbs_link`` build option:

- ``shared`` (default): the PMD depends on some .so files.

- ``dlopen``: Split the dependencies glue in a separate library
  loaded when needed by dlopen.
  It make dependencies on libibverbs and libmlx4 optional,
  and has no performance impact.

- ``static``: Embed static flavor of the dependencies libibverbs and libmlx4
  in the PMD shared library or the executable static binary.


Environment variables
~~~~~~~~~~~~~~~~~~~~~

- ``MLX4_GLUE_PATH``

  A list of directories in which to search for the rdma-core "glue" plug-in,
  separated by colons or semi-colons.


Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- librte_net_mlx4 brings kernel network interfaces up during initialization
  because it is affected by their state. Forcing them down prevents packets
  reception.

- **ethtool** operations on related kernel interfaces also affect the PMD.

- ``port`` parameter [int]

  This parameter provides a physical port to probe and can be specified multiple
  times for additional ports. All ports are probed by default if left
  unspecified.

- ``mr_ext_memseg_en`` parameter [int]

  A nonzero value enables extending memseg when registering DMA memory. If
  enabled, the number of entries in MR (Memory Region) lookup table on datapath
  is minimized and it benefits performance. On the other hand, it worsens memory
  utilization because registered memory is pinned by kernel driver. Even if a
  page in the extended chunk is freed, that doesn't become reusable until the
  entire memory is freed.

  Enabled by default.

Kernel module parameters
~~~~~~~~~~~~~~~~~~~~~~~~

The **mlx4_core** kernel module has several parameters that affect the
behavior and/or the performance of librte_net_mlx4. Some of them are described
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

Limitations
-----------

- For secondary process:

  - Forked secondary process not supported.
  - External memory unregistered in EAL memseg list cannot be used for DMA
    unless such memory has been registered by ``mlx4_mr_update_ext_mp()`` in
    primary process and remapped to the same virtual address in secondary
    process. If the external memory is registered by primary process but has
    different virtual address in secondary process, unexpected error may happen.

- CRC stripping is supported by default and always reported as "true".
  The ability to enable/disable CRC stripping requires OFED version
  4.3-1.5.0.0 and above  or rdma-core version v18 and above.

- TSO (Transmit Segmentation Offload) is supported in OFED version
  4.4 and above.

- RSS only works on power-of-two number of queues.

- It is possible to open non-power-of-two queues,
  but the PMD will round down to the highest power-of-two queues by default for RSS.
  Other queues can be utilized through flow API.
  Example::

      ./dpdk-testpmd -a 08:00.0 -- -i --rxq 12 --txq 12 --rss-ip

  The first 8 queues will be used by default for RSS over IP.
  The rest of the queues can be utilized with flow API like the following::

      flow create 0 ingress pattern eth / ipv4 / tcp / end actions rss queues 8 9 10 11 end / end


Prerequisites
-------------

This driver relies on external libraries and kernel drivers for resources
allocations and initialization. The following dependencies are not part of
DPDK and must be installed separately:

- **libibverbs** (provided by rdma-core package)

  User space verbs framework used by librte_net_mlx4. This library provides
  a generic interface between the kernel and low-level user space drivers
  such as libmlx4.

  It allows slow and privileged operations (context initialization, hardware
  resources allocations) to be managed by the kernel and fast operations to
  never leave user space.

- **libmlx4** (provided by rdma-core package)

  Low-level user space driver library for NVIDIA ConnectX-3 devices,
  it is automatically loaded by libibverbs.

  This library basically implements send/receive calls to the hardware
  queues.

- **Kernel modules**

  They provide the kernel-side verbs API and low level device drivers that
  manage actual hardware initialization and resources sharing with user
  space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to
  their devices:

  - mlx4_core: hardware driver managing NVIDIA ConnectX-3 devices.
  - mlx4_en: Ethernet device driver that provides kernel network interfaces.
  - mlx4_ib: InfiniBand device driver.
  - ib_uverbs: user space driver for verbs (entry point for libibverbs).

- **Firmware update**

  NVIDIA MLNX_OFED releases include firmware updates for ConnectX-3 adapters.

  Because each release provides new features, these updates must be applied to
  match the kernel modules and libraries they come with.

.. note::

   Both libraries are BSD and GPL licensed. Linux kernel modules are GPL
   licensed.

Depending on system constraints and user preferences either RDMA core library
with a recent enough Linux kernel release (recommended) or NVIDIA MLNX_OFED,
which provides compatibility with older releases.

Current RDMA core package and Linux kernel (recommended)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Minimal Linux kernel version: 4.14.
- Minimal RDMA core version: v15 (see `RDMA core installation documentation`_).

- Starting with rdma-core v21, static libraries can be built::

    cd build
    CFLAGS=-fPIC cmake -DENABLE_STATIC=1 -DNO_PYVERBS=1 -DNO_MAN_PAGES=1 -GNinja ..
    ninja
    ninja install

.. _`RDMA core installation documentation`: https://raw.githubusercontent.com/linux-rdma/rdma-core/master/README.md

.. _OFED_as_a_fallback:

NVIDIA MLNX_OFED as a fallback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- `NVIDIA MLNX_OFED`_ version: **4.4, 4.5, 4.6**.
- firmware version: **2.42.5000** and above.

.. _`NVIDIA MLNX_OFED`: https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/

.. note::

   Several versions of NVIDIA MLNX_OFED are available. Installing the version
   this DPDK release was developed and tested against is strongly
   recommended. Please check the `prerequisites`_.

Installing NVIDIA MLNX_OFED
^^^^^^^^^^^^^^^^^^^^^^^^^^^

#. Download latest NVIDIA MLNX_OFED.

#. Install the required libraries and kernel modules either by installing
   only the required set, or by installing the entire NVIDIA MLNX_OFED:

   For bare metal use::

        ./mlnxofedinstall --dpdk --upstream-libs

   For SR-IOV hypervisors use::

        ./mlnxofedinstall --dpdk --upstream-libs --enable-sriov --hypervisor

   For SR-IOV virtual machine use::

        ./mlnxofedinstall --dpdk --upstream-libs --guest

#. Verify the firmware is the correct one::

        ibv_devinfo

#. Set all ports links to Ethernet, follow instructions on the screen::

        connectx_port_config

#. Continue with :ref:`section 2 of the Quick Start Guide <QSG_2>`.

.. _qsg:

Quick Start Guide
-----------------

#. Set all ports links to Ethernet::

        PCI=<NIC PCI address>
        echo eth > "/sys/bus/pci/devices/$PCI/mlx4_port0"
        echo eth > "/sys/bus/pci/devices/$PCI/mlx4_port1"

   .. note::

        If using NVIDIA MLNX_OFED one can permanently set the port link
        to Ethernet using connectx_port_config tool provided by it.
        :ref:`OFED_as_a_fallback`:

.. _QSG_2:

#. In case of bare metal or hypervisor, configure optimized steering mode
   by adding the following line to ``/etc/modprobe.d/mlx4_core.conf``::

        options mlx4_core log_num_mgm_entry_size=-7

   .. note::

        If VLAN filtering is used, set log_num_mgm_entry_size=-1.
        Performance degradation can occur on this case.

#. Restart the driver::

        /etc/init.d/openibd restart

   or::

        service openibd restart

#. Install DPDK and you are ready to go.
   See :doc:`compilation instructions <../linux_gsg/build_dpdk>`.

Performance tuning
------------------

#. Verify the optimized steering mode is configured::

        cat /sys/module/mlx4_core/parameters/log_num_mgm_entry_size

#. Use the CPU near local NUMA node to which the PCIe adapter is connected,
   for better performance. For VMs, verify that the right CPU
   and NUMA node are pinned according to the above. Run::

        lstopo-no-graphics --merge

   to identify the NUMA node to which the PCIe adapter is connected.

#. If more than one adapter is used, and root complex capabilities allow
   to put both adapters on the same NUMA node without PCI bandwidth degradation,
   it is recommended to locate both adapters on the same NUMA node.
   This in order to forward packets from one to the other without
   NUMA performance penalty.

#. Disable pause frames::

        ethtool -A <netdev> rx off tx off

#. Verify IO non-posted prefetch is disabled by default. This can be checked
   via the BIOS configuration. Please contact you server provider for more
   information about the settings.

   .. note::

        On some machines, depends on the machine integrator, it is beneficial
        to set the PCI max read request parameter to 1K. This can be
        done in the following way:

        To query the read request size use::

                setpci -s <NIC PCI address> 68.w

        If the output is different than 3XXX, set it by::

                setpci -s <NIC PCI address> 68.w=3XXX

        The XXX can be different on different systems. Make sure to configure
        according to the setpci output.

#. To minimize overhead of searching Memory Regions:

   - '--socket-mem' is recommended to pin memory by predictable amount.
   - Configure per-lcore cache when creating Mempools for packet buffer.
   - Refrain from dynamically allocating/freeing memory in run-time.

Usage example
-------------

This section demonstrates how to launch **testpmd** with NVIDIA ConnectX-3
devices managed by librte_net_mlx4.

#. Load the kernel modules::

      modprobe -a ib_uverbs mlx4_en mlx4_core mlx4_ib

   Alternatively if MLNX_OFED is fully installed, the following script can
   be run::

      /etc/init.d/openibd restart

   .. note::

      User space I/O kernel modules (uio and igb_uio) are not used and do
      not have to be loaded.

#. Make sure Ethernet interfaces are in working order and linked to kernel
   verbs. Related sysfs entries should be present::

      ls -d /sys/class/net/*/device/infiniband_verbs/uverbs* | cut -d / -f 5

   Example output::

      eth2
      eth3
      eth4
      eth5

#. Optionally, retrieve their PCI bus addresses to be used with the allow argument::

      {
          for intf in eth2 eth3 eth4 eth5;
          do
              (cd "/sys/class/net/${intf}/device/" && pwd -P);
          done;
      } |
      sed -n 's,.*/\(.*\),-a \1,p'

   Example output::

      -a 0000:83:00.0
      -a 0000:83:00.0
      -a 0000:84:00.0
      -a 0000:84:00.0

   .. note::

      There are only two distinct PCI bus addresses because the NVIDIA
      ConnectX-3 adapters installed on this system are dual port.

#. Request huge pages::

      dpdk-hugepages.py --setup 2G

#. Start testpmd with basic parameters::

      dpdk-testpmd -l 8-15 -n 4 -a 0000:83:00.0 -a 0000:84:00.0 -- --rxq=2 --txq=2 -i

   Example output::

      [...]
      EAL: PCI device 0000:83:00.0 on NUMA socket 1
      EAL:   probe driver: 15b3:1007 librte_net_mlx4
      PMD: librte_net_mlx4: PCI information matches, using device "mlx4_0" (VF: false)
      PMD: librte_net_mlx4: 2 port(s) detected
      PMD: librte_net_mlx4: port 1 MAC address is 00:02:c9:b5:b7:50
      PMD: librte_net_mlx4: port 2 MAC address is 00:02:c9:b5:b7:51
      EAL: PCI device 0000:84:00.0 on NUMA socket 1
      EAL:   probe driver: 15b3:1007 librte_net_mlx4
      PMD: librte_net_mlx4: PCI information matches, using device "mlx4_1" (VF: false)
      PMD: librte_net_mlx4: 2 port(s) detected
      PMD: librte_net_mlx4: port 1 MAC address is 00:02:c9:b5:ba:b0
      PMD: librte_net_mlx4: port 2 MAC address is 00:02:c9:b5:ba:b1
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      PMD: librte_net_mlx4: 0x867d60: TX queues number update: 0 -> 2
      PMD: librte_net_mlx4: 0x867d60: RX queues number update: 0 -> 2
      Port 0: 00:02:C9:B5:B7:50
      Configuring Port 1 (socket 0)
      PMD: librte_net_mlx4: 0x867da0: TX queues number update: 0 -> 2
      PMD: librte_net_mlx4: 0x867da0: RX queues number update: 0 -> 2
      Port 1: 00:02:C9:B5:B7:51
      Configuring Port 2 (socket 0)
      PMD: librte_net_mlx4: 0x867de0: TX queues number update: 0 -> 2
      PMD: librte_net_mlx4: 0x867de0: RX queues number update: 0 -> 2
      Port 2: 00:02:C9:B5:BA:B0
      Configuring Port 3 (socket 0)
      PMD: librte_net_mlx4: 0x867e20: TX queues number update: 0 -> 2
      PMD: librte_net_mlx4: 0x867e20: RX queues number update: 0 -> 2
      Port 3: 00:02:C9:B5:BA:B1
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 40000 Mbps - full-duplex
      Port 2 Link Up - speed 10000 Mbps - full-duplex
      Port 3 Link Up - speed 40000 Mbps - full-duplex
      Done
      testpmd>
