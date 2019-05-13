..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2012 6WIND S.A.
    Copyright 2015 Mellanox Technologies, Ltd

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

The :ref:`flow_isolated_mode` is supported.

Compiling librte_pmd_mlx4 causes DPDK to be linked against libibverbs.

Configuration
-------------

Compilation options
~~~~~~~~~~~~~~~~~~~

These options can be modified in the ``.config`` file.

- ``CONFIG_RTE_LIBRTE_MLX4_PMD`` (default **n**)

  Toggle compilation of librte_pmd_mlx4 itself.

- ``CONFIG_RTE_IBVERBS_LINK_DLOPEN`` (default **n**)

  Build PMD with additional code to make it loadable without hard
  dependencies on **libibverbs** nor **libmlx4**, which may not be installed
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

- ``CONFIG_RTE_IBVERBS_LINK_STATIC`` (default **n**)

  Embed static flavor of the dependencies **libibverbs** and **libmlx4**
  in the PMD shared library or the executable static binary.

- ``CONFIG_RTE_LIBRTE_MLX4_DEBUG`` (default **n**)

  Toggle debugging code and stricter compilation flags. Enabling this option
  adds additional run-time checks and debugging messages at the cost of
  lower performance.

Environment variables
~~~~~~~~~~~~~~~~~~~~~

- ``MLX4_GLUE_PATH``

  A list of directories in which to search for the rdma-core "glue" plug-in,
  separated by colons or semi-colons.

  Only matters when compiled with ``CONFIG_RTE_IBVERBS_LINK_DLOPEN``
  enabled and most useful when ``CONFIG_RTE_EAL_PMD_PATH`` is also set,
  since ``LD_LIBRARY_PATH`` has no effect in this case.

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- librte_pmd_mlx4 brings kernel network interfaces up during initialization
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

Prerequisites
-------------

This driver relies on external libraries and kernel drivers for resources
allocations and initialization. The following dependencies are not part of
DPDK and must be installed separately:

- **libibverbs** (provided by rdma-core package)

  User space verbs framework used by librte_pmd_mlx4. This library provides
  a generic interface between the kernel and low-level user space drivers
  such as libmlx4.

  It allows slow and privileged operations (context initialization, hardware
  resources allocations) to be managed by the kernel and fast operations to
  never leave user space.

- **libmlx4** (provided by rdma-core package)

  Low-level user space driver library for Mellanox ConnectX-3 devices,
  it is automatically loaded by libibverbs.

  This library basically implements send/receive calls to the hardware
  queues.

- **Kernel modules**

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

Depending on system constraints and user preferences either RDMA core library
with a recent enough Linux kernel release (recommended) or Mellanox OFED,
which provides compatibility with older releases.

Current RDMA core package and Linux kernel (recommended)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Minimal Linux kernel version: 4.14.
- Minimal RDMA core version: v15 (see `RDMA core installation documentation`_).

- Starting with rdma-core v21, static libraries can be built::

    cd build
    CFLAGS=-fPIC cmake -DIN_PLACE=1 -DENABLE_STATIC=1 -GNinja ..
    ninja

.. _`RDMA core installation documentation`: https://raw.githubusercontent.com/linux-rdma/rdma-core/master/README.md

If rdma-core libraries are built but not installed, DPDK makefile can link them,
thanks to these environment variables:

   - ``EXTRA_CFLAGS=-I/path/to/rdma-core/build/include``
   - ``EXTRA_LDFLAGS=-L/path/to/rdma-core/build/lib``
   - ``PKG_CONFIG_PATH=/path/to/rdma-core/build/lib/pkgconfig``

.. _Mellanox_OFED_as_a_fallback:

Mellanox OFED as a fallback
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- `Mellanox OFED`_ version: **4.4, 4.5, 4.6**.
- firmware version: **2.42.5000** and above.

.. _`Mellanox OFED`: http://www.mellanox.com/page/products_dyn?product_family=26&mtag=linux_sw_drivers

.. note::

   Several versions of Mellanox OFED are available. Installing the version
   this DPDK release was developed and tested against is strongly
   recommended. Please check the `prerequisites`_.

Installing Mellanox OFED
^^^^^^^^^^^^^^^^^^^^^^^^

1. Download latest Mellanox OFED.

2. Install the required libraries and kernel modules either by installing
   only the required set, or by installing the entire Mellanox OFED:

   For bare metal use:

   .. code-block:: console

        ./mlnxofedinstall --dpdk --upstream-libs

   For SR-IOV hypervisors use:

   .. code-block:: console

        ./mlnxofedinstall --dpdk --upstream-libs --enable-sriov --hypervisor

   For SR-IOV virtual machine use:

   .. code-block:: console

        ./mlnxofedinstall --dpdk --upstream-libs --guest

3. Verify the firmware is the correct one:

   .. code-block:: console

        ibv_devinfo

4. Set all ports links to Ethernet, follow instructions on the screen:

   .. code-block:: console

        connectx_port_config

5. Continue with :ref:`section 2 of the Quick Start Guide <QSG_2>`.

Supported NICs
--------------

* Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2*40G)

.. _qsg:

Quick Start Guide
-----------------

1. Set all ports links to Ethernet

   .. code-block:: console

        PCI=<NIC PCI address>
        echo eth > "/sys/bus/pci/devices/$PCI/mlx4_port0"
        echo eth > "/sys/bus/pci/devices/$PCI/mlx4_port1"

   .. note::

        If using Mellanox OFED one can permanently set the port link
        to Ethernet using connectx_port_config tool provided by it.
        :ref:`Mellanox_OFED_as_a_fallback`:

.. _QSG_2:

2. In case of bare metal or hypervisor, configure optimized steering mode
   by adding the following line to ``/etc/modprobe.d/mlx4_core.conf``:

   .. code-block:: console

        options mlx4_core log_num_mgm_entry_size=-7

   .. note::

        If VLAN filtering is used, set log_num_mgm_entry_size=-1.
        Performance degradation can occur on this case.

3. Restart the driver:

   .. code-block:: console

        /etc/init.d/openibd restart

   or:

   .. code-block:: console

        service openibd restart

4. Compile DPDK and you are ready to go. See instructions on
   :ref:`Development Kit Build System <Development_Kit_Build_System>`

Performance tuning
------------------

1. Verify the optimized steering mode is configured:

  .. code-block:: console

        cat /sys/module/mlx4_core/parameters/log_num_mgm_entry_size

2. Use the CPU near local NUMA node to which the PCIe adapter is connected,
   for better performance. For VMs, verify that the right CPU
   and NUMA node are pinned according to the above. Run:

   .. code-block:: console

        lstopo-no-graphics

   to identify the NUMA node to which the PCIe adapter is connected.

3. If more than one adapter is used, and root complex capabilities allow
   to put both adapters on the same NUMA node without PCI bandwidth degradation,
   it is recommended to locate both adapters on the same NUMA node.
   This in order to forward packets from one to the other without
   NUMA performance penalty.

4. Disable pause frames:

   .. code-block:: console

        ethtool -A <netdev> rx off tx off

5. Verify IO non-posted prefetch is disabled by default. This can be checked
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

6. To minimize overhead of searching Memory Regions:

   - '--socket-mem' is recommended to pin memory by predictable amount.
   - Configure per-lcore cache when creating Mempools for packet buffer.
   - Refrain from dynamically allocating/freeing memory in run-time.

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
