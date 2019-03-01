..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

System Requirements
===================

This chapter describes the packages required to compile the DPDK.

.. note::

    If the DPDK is being used on an Intel® Communications Chipset 89xx Series platform,
    please consult the *Intel® Communications Chipset 89xx Series Software for Linux Getting Started Guide*.

BIOS Setting Prerequisite on x86
--------------------------------

For the majority of platforms, no special BIOS settings are needed to use basic DPDK functionality.
However, for additional HPET timer and power management functionality,
and high performance of small packets, BIOS setting changes may be needed.
Consult the section on :ref:`Enabling Additional Functionality <Enabling_Additional_Functionality>`
for more information on the required changes.

.. note::

   If UEFI secure boot is enabled, the Linux kernel may disallow the use of
   UIO on the system. Therefore, devices for use by DPDK should be bound to the
   ``vfio-pci`` kernel module rather than ``igb_uio`` or ``uio_pci_generic``.
   For more details see :ref:`linux_gsg_binding_kernel`.

Compilation of the DPDK
-----------------------

**Required Tools and Libraries:**

.. note::

    The setup commands and installed packages needed on various systems may be different.
    For details on Linux distributions and the versions tested, please consult the DPDK Release Notes.

*   GNU ``make``.

*   coreutils: ``cmp``, ``sed``, ``grep``, ``arch``, etc.

*   gcc: versions 4.9 or later is recommended for all platforms.
    On some distributions, some specific compiler flags and linker flags are enabled by
    default and affect performance (``-fstack-protector``, for example). Please refer to the documentation
    of your distribution and to ``gcc -dumpspecs``.

*   libc headers, often packaged as ``gcc-multilib`` (``glibc-devel.i686`` / ``libc6-dev-i386``;
    ``glibc-devel.x86_64`` / ``libc6-dev`` for 64-bit compilation on Intel architecture;
    ``glibc-devel.ppc64`` for 64 bit IBM Power architecture;)

*   Linux kernel headers or sources required to build kernel modules. (kernel - devel.x86_64;
    kernel - devel.ppc64)

*   Additional packages required for 32-bit compilation on 64-bit systems are:

    * glibc.i686, libgcc.i686, libstdc++.i686 and glibc-devel.i686 for Intel i686/x86_64;

    * glibc.ppc64, libgcc.ppc64, libstdc++.ppc64 and glibc-devel.ppc64 for IBM ppc_64;

    .. note::

       x86_x32 ABI is currently supported with distribution packages only on Ubuntu
       higher than 13.10 or recent Debian distribution. The only supported  compiler is gcc 4.9+.

*   Library for handling NUMA (Non Uniform Memory Access).

    * numactl-devel in Red Hat/Fedora;

    * libnuma-dev in Debian/Ubuntu;

    .. note::

        On systems with NUMA support, `libnuma-dev` (aka `numactl-devel`)
        is a recommended dependency when `--legacy-mem` switch is used,
        and a *required* dependency if default memory mode is used.
        While DPDK will compile and run without `libnuma`
        even on NUMA-enabled systems,
        both usability and performance will be degraded.

*   Python, version 2.7+ or 3.2+, to use various helper scripts included in the DPDK package.


**Optional Tools:**

*   Intel® C++ Compiler (icc). For installation, additional libraries may be required.
    See the icc Installation Guide found in the Documentation directory under the compiler installation.

*   IBM® Advance ToolChain for Powerlinux. This is a set of open source development tools and runtime libraries
    which allows users to take leading edge advantage of IBM's latest POWER hardware features on Linux. To install
    it, see the IBM official installation document.

*   libpcap headers and libraries (libpcap-devel) to compile and use the libpcap-based poll-mode driver.
    This driver is disabled by default and can be enabled by setting ``CONFIG_RTE_LIBRTE_PMD_PCAP=y`` in the build time config file.

*   libarchive headers and library are needed for some unit tests using tar to get their resources.


Running DPDK Applications
-------------------------

To run an DPDK application, some customization may be required on the target machine.

System Software
~~~~~~~~~~~~~~~

**Required:**

*   Kernel version >= 3.16

    The kernel version required is based on the oldest long term stable kernel available
    at kernel.org when the DPDK version is in development.
    Compatibility for recent distribution kernels will be kept, notably RHEL/CentOS 7.

    The kernel version in use can be checked using the command::

        uname -r

*   glibc >= 2.7 (for features related to cpuset)

    The version can be checked using the ``ldd --version`` command.

*   Kernel configuration

    In the Fedora OS and other common distributions, such as Ubuntu, or Red Hat Enterprise Linux,
    the vendor supplied kernel configurations can be used to run most DPDK applications.

    For other kernel builds, options which should be enabled for DPDK include:

    *   HUGETLBFS

    *   PROC_PAGE_MONITOR  support

    *   HPET and HPET_MMAP configuration options should also be enabled if HPET  support is required.
        See the section on :ref:`High Precision Event Timer (HPET) Functionality <High_Precision_Event_Timer>` for more details.

.. _linux_gsg_hugepages:

Use of Hugepages in the Linux Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Hugepage support is required for the large memory pool allocation used for packet buffers
(the HUGETLBFS option must be enabled in the running kernel as indicated the previous section).
By using hugepage allocations, performance is increased since fewer pages are needed,
and therefore less Translation Lookaside Buffers (TLBs, high speed translation caches),
which reduce the time it takes to translate a virtual page address to a physical page address.
Without hugepages, high TLB miss rates would occur with the standard 4k page size, slowing performance.

Reserving Hugepages for DPDK Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The allocation of hugepages should be done at boot time or as soon as possible after system boot
to prevent memory from being fragmented in physical memory.
To reserve hugepages at boot time, a parameter is passed to the Linux kernel on the kernel command line.

For 2 MB pages, just pass the hugepages option to the kernel. For example, to reserve 1024 pages of 2 MB, use::

    hugepages=1024

For other hugepage sizes, for example 1G pages, the size must be specified explicitly and
can also be optionally set as the default hugepage size for the system.
For example, to reserve 4G of hugepage memory in the form of four 1G pages, the following options should be passed to the kernel::

    default_hugepagesz=1G hugepagesz=1G hugepages=4

.. note::

    The hugepage sizes that a CPU supports can be determined from the CPU flags on Intel architecture.
    If pse exists, 2M hugepages are supported; if pdpe1gb exists, 1G hugepages are supported.
    On IBM Power architecture, the supported hugepage sizes are 16MB and 16GB.

.. note::

    For 64-bit applications, it is recommended to use 1 GB hugepages if the platform supports them.

In the case of a dual-socket NUMA system,
the number of hugepages reserved at boot time is generally divided equally between the two sockets
(on the assumption that sufficient memory is present on both sockets).

See the Documentation/admin-guide/kernel-parameters.txt file in your Linux source tree for further details of these and other kernel options.

**Alternative:**

For 2 MB pages, there is also the option of allocating hugepages after the system has booted.
This is done by echoing the number of hugepages required to a nr_hugepages file in the ``/sys/devices/`` directory.
For a single-node system, the command to use is as follows (assuming that 1024 pages are required)::

    echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

On a NUMA machine, pages should be allocated explicitly on separate nodes::

    echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
    echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages

.. note::

    For 1G pages, it is not possible to reserve the hugepage memory after the system has booted.

Using Hugepages with the DPDK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once the hugepage memory is reserved, to make the memory available for DPDK use, perform the following steps::

    mkdir /mnt/huge
    mount -t hugetlbfs nodev /mnt/huge

The mount point can be made permanent across reboots, by adding the following line to the ``/etc/fstab`` file::

    nodev /mnt/huge hugetlbfs defaults 0 0

For 1GB pages, the page size must be specified as a mount option::

    nodev /mnt/huge_1GB hugetlbfs pagesize=1GB 0 0
