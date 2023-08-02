..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. include:: <isonum.txt>

System Requirements
===================

This chapter describes the packages required to compile the DPDK.

BIOS Setting Prerequisite on x86
--------------------------------

For the majority of platforms, no special BIOS settings are needed to use basic DPDK functionality.
However, for additional HPET timer and power management functionality,
and high performance of small packets, BIOS setting changes may be needed.
Consult the section on :ref:`Enabling Additional Functionality <Enabling_Additional_Functionality>`
for more information on the required changes.

Compilation of the DPDK
-----------------------

**Required Tools and Libraries:**

.. note::

    The setup commands and installed packages needed on various systems may be different.
    For details on Linux distributions and the versions tested, please consult the DPDK Release Notes.

*   General development tools including a C compiler supporting the C11 standard,
    including standard atomics, for example: GCC (version 5.0+) or Clang (version 3.6+),
    and ``pkg-config`` or ``pkgconf`` to be used when building end-user binaries against DPDK.

    * For RHEL/Fedora systems these can be installed using ``dnf groupinstall "Development Tools"``
    * For Ubuntu/Debian systems these can be installed using ``apt install build-essential``
    * For Alpine Linux, ``apk add alpine-sdk bsd-compat-headers``

.. note::

   pkg-config 0.27, supplied with RHEL-7,
   does not process the Libs.private section correctly,
   resulting in statically linked applications not being linked properly.
   Use an updated version of ``pkg-config`` or ``pkgconf`` instead when building applications

*   Python 3.6 or later.

*   Meson (version 0.53.2+) and ninja

    * ``meson`` & ``ninja-build`` packages in most Linux distributions
    * If the packaged version is below the minimum version, the latest versions
      can be installed from Python's "pip" repository: ``pip3 install meson ninja``

*   ``pyelftools`` (version 0.22+)

    * For Fedora systems it can be installed using ``dnf install python-pyelftools``
    * For RHEL/CentOS systems it can be installed using ``pip3 install pyelftools``
    * For Ubuntu/Debian it can be installed using ``apt install python3-pyelftools``
    * For Alpine Linux, ``apk add py3-elftools``

*   Library for handling NUMA (Non Uniform Memory Access).

    * ``numactl-devel`` in RHEL/Fedora;
    * ``libnuma-dev`` in Debian/Ubuntu;
    * ``numactl-dev`` in Alpine Linux

.. note::

   Please ensure that the latest patches are applied to third party libraries
   and software to avoid any known vulnerabilities.


**Optional Tools:**

*   Intel\ |reg| C++ Compiler (icc). For installation, additional libraries may be required.
    See the icc Installation Guide found in the Documentation directory under the compiler installation.

*   IBM\ |reg| Advance ToolChain for Powerlinux. This is a set of open source development tools and runtime libraries
    which allows users to take leading edge advantage of IBM's latest POWER hardware features on Linux. To install
    it, see the IBM official installation document.

**Additional Libraries**

A number of DPDK components, such as libraries and poll-mode drivers (PMDs) have additional dependencies.
For DPDK builds, the presence or absence of these dependencies will be automatically detected
enabling or disabling the relevant components appropriately.

In each case, the relevant library development package (``-devel`` or ``-dev``) is needed to build the DPDK components.

For libraries the additional dependencies include:

*   libarchive: for some unit tests using tar to get their resources.

*   libelf: to compile and use the bpf library.

For poll-mode drivers, the additional dependencies for each driver can be
found in that driver's documentation in the relevant DPDK guide document,
e.g. :doc:`../nics/index`

Running DPDK Applications
-------------------------

To run a DPDK application, some customization may be required on the target machine.

System Software
~~~~~~~~~~~~~~~

**Required:**

*   Kernel version >= 4.14

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

The reservation of hugepages can be performed at run time.
This is done by echoing the number of hugepages required
to a ``nr_hugepages`` file in the ``/sys/kernel/`` directory
corresponding to a specific page size (in Kilobytes).
For a single-node system, the command to use is as follows
(assuming that 1024 of 2MB pages are required)::

    echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

On a NUMA machine, the above command will usually divide the number of hugepages
equally across all NUMA nodes (assuming there is enough memory on all NUMA nodes).
However, pages can also be reserved explicitly on individual NUMA nodes
using a ``nr_hugepages`` file in the ``/sys/devices/`` directory::

    echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
    echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages

The tool ``dpdk-hugepages.py`` can be used to manage hugepages.

.. note::

    Some kernel versions may not allow reserving 1 GB hugepages at run time,
    so reserving them at boot time may be the only option.
    Please see below for instructions.

**Alternative:**

In the general case, reserving hugepages at run time is perfectly fine,
but in use cases where having lots of physically contiguous memory is required,
it is preferable to reserve hugepages at boot time,
as that will help in preventing physical memory from becoming heavily fragmented.

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

Using Hugepages with the DPDK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If secondary process support is not required, DPDK is able to use hugepages
without any configuration by using "in-memory" mode.
Please see :doc:`linux_eal_parameters` for more details.

If secondary process support is required,
mount points for hugepages need to be created.
On modern Linux distributions, a default mount point for hugepages
is provided by the system and is located at ``/dev/hugepages``.
This mount point will use the default hugepage size
set by the kernel parameters as described above.

However, in order to use hugepage sizes other than the default, it is necessary
to manually create mount points for those hugepage sizes (e.g. 1GB pages).

To make the hugepages of size 1GB available for DPDK use,
following steps must be performed::

    mkdir /mnt/huge
    mount -t hugetlbfs pagesize=1GB /mnt/huge

The mount point can be made permanent across reboots, by adding the following line to the ``/etc/fstab`` file::

    nodev /mnt/huge hugetlbfs pagesize=1GB 0 0
