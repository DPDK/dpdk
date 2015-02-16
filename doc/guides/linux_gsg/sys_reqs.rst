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

System Requirements
===================

This chapter describes the packages required to compile the DPDK.

.. note::

    If the DPDK is being used on an Intel® Communications Chipset 89xx Series platform,
    please consult the *Intel® Communications Chipset 89xx Series Software for Linux* Getting Started Guide*.

BIOS Setting Prerequisite on x86
--------------------------------

For the majority of platforms, no special BIOS settings are needed to use basic DPDK functionality.
However, for additional HPET timer and power management functionality,
and high performance of small packets on 40G NIC, BIOS setting changes may be needed.
Consult :ref:`Chapter 5. Enabling Additional Functionality <Enabling_Additional_Functionality>`
for more information on the required changes.

Compilation of the DPDK
-----------------------

**Required Tools:**

.. note::

    Testing has been performed using Fedora* 18. The setup commands and installed packages needed on other systems may be different.
    For details on other Linux distributions and the versions tested, please consult the DPDK Release Notes.

*   GNU  make

*   coreutils:  cmp, sed, grep, arch

*   gcc: versions 4.5.x or later is recommended for i686/x86_64. versions 4.8.x or later is recommanded
    for ppc_64 and x86_x32 ABI. On some distributions, some specific compiler flags and linker flags are enabled by
    default and affect performance (- fstack-protector, for example). Please refer to the documentation
    of your distribution and to gcc -dumpspecs.

*   libc headers (glibc-devel.i686 / libc6-dev-i386; glibc-devel.x86_64 for 64-bit compilation on Intel
    architecture; glibc-devel.ppc64 for 64 bit IBM Power architecture;)

*   Linux kernel headers or sources required to build kernel modules. (kernel - devel.x86_64;
    kernel - devel.ppc64)

*   Additional packages required for 32-bit compilation on 64-bit systems are:

    glibc.i686, libgcc.i686, libstdc++.i686 and glibc-devel.i686 for Intel i686/x86_64;

    glibc.ppc64, libgcc.ppc64, libstdc++.ppc64 and glibc-devel.ppc64 for IBM ppc_64;

.. note::

    x86_x32 ABI is currently supported with distribution packages only on Ubuntu
    higher than 13.10 or recent debian distribution. The only supported  compiler is gcc 4.8+.

.. note::

    Python, version 2.6 or 2.7, to use various helper scripts included in the DPDK package


**Optional Tools:**

*   Intel®  C++ Compiler (icc). For installation, additional libraries may be required.
    See the icc Installation Guide found in the Documentation directory under the compiler installation.
    This release has been tested using version 12.1.

*   IBM® Advance ToolChain for Powerlinux. This is a set of open source development tools and runtime libraries
    which allows users to take leading edge advantage of IBM's latest POWER hardware features on Linux. To install
    it, see the IBM official installation document.

*   libpcap headers and libraries (libpcap-devel) to compile and use the libpcap-based poll-mode driver.
    This driver is disabled by default and can be enabled by setting CONFIG_RTE_LIBRTE_PMD_PCAP=y in the build time config file.

Running DPDK Applications
-------------------------

To run an DPDK application, some customization may be required on the target machine.

System Software
~~~~~~~~~~~~~~~

**Required:**

*   Kernel version >= 2.6.33

    The kernel version in use can be checked using the command:

    .. code-block:: console

        uname -r

For details of the patches needed to use the DPDK with earlier kernel versions,
see the DPDK FAQ included in the *DPDK Release Notes*.
Note also that Redhat* Linux* 6.2 and 6.3 uses a 2.6.32 kernel that already has all the necessary patches applied.

*   glibc >= 2.7 (for features related to cpuset)

    The version can be checked using the ldd --version command. A sample output is shown below:

    .. code-block:: console

        # ldd --version

        ldd (GNU libc) 2.14.90
        Copyright (C) 2011 Free Software Foundation, Inc.
        This is free software; see the source for copying conditions. There is NO
        warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
        Written by Roland McGrath and Ulrich Drepper.

*   Kernel configuration

    In the Fedora* OS and other common distributions, such as Ubuntu*, or RedHat Enterprise Linux*,
    the vendor supplied kernel configurations can be used to run most DPDK applications.

    For other kernel builds, options which should be enabled for DPDK include:

    *   UIO support

    *   HUGETLBFS

    *   PROC_PAGE_MONITOR  support

    *   HPET and HPET_MMAP configuration options should also be enabled if HPET  support is required.
        See :ref:`Section 5.1 High Precision Event Timer (HPET) Functionality <High_Precision_Event_Timer>` for more details.

Use of Hugepages in the Linux* Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Hugepage support is required for the large memory pool allocation used for packet buffers
(the HUGETLBFS option must be enabled in the running kernel as indicated in Section 2.3).
By using hugepage allocations, performance is increased since fewer pages are needed,
and therefore less Translation Lookaside Buffers (TLBs, high speed translation caches),
which reduce the time it takes to translate a virtual page address to a physical page address.
Without hugepages, high TLB miss rates would occur with the standard 4k page size, slowing performance.

Reserving Hugepages for DPDK Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The allocation of hugepages should be done at boot time or as soon as possible after system boot
to prevent memory from being fragmented in physical memory.
To reserve hugepages at boot time, a parameter is passed to the Linux* kernel on the kernel command line.

For 2 MB pages, just pass the hugepages option to the kernel. For example, to reserve 1024 pages of 2 MB, use:

.. code-block:: console

    hugepages=1024

For other hugepage sizes, for example 1G pages, the size must be specified explicitly and
can also be optionally set as the default hugepage size for the system.
For example, to reserve 4G of hugepage memory in the form of four 1G pages, the following options should be passed to the kernel:

.. code-block:: console

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

See the Documentation/kernel-parameters.txt file in your Linux* source tree for further details of these and other kernel options.

**Alternative:**

For 2 MB pages, there is also the option of allocating hugepages after the system has booted.
This is done by echoing the number of hugepages required to a nr_hugepages file in the /sys/devices/ directory.
For a single-node system, the command to use is as follows (assuming that 1024 pages are required):

.. code-block:: console

    echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

On a NUMA machine, pages should be allocated explicitly on separate nodes:

.. code-block:: console

    echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
    echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages

.. note::

    For 1G pages, it is not possible to reserve the hugepage memory after the system has booted.

Using Hugepages with the DPDK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once the hugepage memory is reserved, to make the memory available for DPDK use, perform the following steps:

.. code-block:: console

    mkdir /mnt/huge
    mount -t hugetlbfs nodev /mnt/huge

The mount point can be made permanent across reboots, by adding the following line to the /etc/fstab file:

.. code-block:: console

    nodev /mnt/huge hugetlbfs defaults 0 0

For 1GB pages, the page size must be specified as a mount option:

.. code-block:: console

    nodev /mnt/huge_1GB hugetlbfs pagesize=1GB 0 0

Xen Domain0 Support in the Linux* Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The existing memory management implementation is based on the Linux* kernel hugepage mechanism.
On the Xen hypervisor, hugepage support for DomainU (DomU) Guests means that DPDK applications work as normal for guests.

However, Domain0 (Dom0) does not support hugepages.
To work around this limitation, a new kernel module rte_dom0_mm is added to facilitate the allocation and mapping of memory via
**IOCTL** (allocation) and **MMAP** (mapping).

Enabling Xen Dom0 Mode in the DPDK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

By default, Xen Dom0 mode is disabled in the DPDK build configuration files.
To support Xen Dom0, the CONFIG_RTE_LIBRTE_XEN_DOM0 setting should be changed to “y”, which enables the Xen Dom0 mode at compile time.

Furthermore, the CONFIG_RTE_EAL_ALLOW_INV_SOCKET_ID setting should also be changed to “y” in the case of the wrong socket ID being received.

Loading the DPDK rte_dom0_mm Module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To run any DPDK application on Xen Dom0, the rte_dom0_mm module must be loaded into the running kernel with rsv_memsize option.
The module is found in the kmod sub-directory of the DPDK target directory.
This module should be loaded using the insmod command as shown below (assuming that the current directory is the DPDK target directory):

.. code-block:: console

    sudo insmod kmod/rte_dom0_mm.ko rsv_memsize=X

The value X cannot be greater than 4096(MB).

Configuring Memory for DPDK Use
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

After the rte_dom0_mm.ko kernel module has been loaded, the user must configure the memory size for DPDK usage.
This is done by echoing the memory size to a memsize file in the /sys/devices/ directory.
Use the following command (assuming that 2048 MB is required):

.. code-block:: console

    echo 2048 > /sys/kernel/mm/dom0-mm/memsize-mB/memsize

The user can also check how much memory has already been used:

.. code-block:: console

    cat /sys/kernel/mm/dom0-mm/memsize-mB/memsize_rsvd

Xen Domain0 does not support NUMA configuration, as a result the --socket-mem command line option is invalid for Xen Domain0.

.. note::

    The memsize value cannot be greater than the rsv_memsize value.

Running the DPDK Application on Xen Domain0
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To run the DPDK application on Xen Domain0, an extra command line option --xen-dom0 is required.
