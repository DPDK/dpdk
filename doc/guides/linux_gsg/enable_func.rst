..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. include:: <isonum.txt>

.. _Enabling_Additional_Functionality:

Enabling Additional Functionality
=================================

.. _High_Precision_Event_Timer:

High Precision Event Timer (HPET) Functionality
-----------------------------------------------

BIOS Support
~~~~~~~~~~~~

The High Precision Timer (HPET) must be enabled in the platform BIOS if the HPET is to be used.
Otherwise, the Time Stamp Counter (TSC) is used by default.
The BIOS is typically accessed by pressing F2 while the platform is starting up.
The user can then navigate to the HPET option. On the Crystal Forest platform BIOS, the path is:
**Advanced -> PCH-IO Configuration -> High Precision Timer ->** (Change from Disabled to Enabled if necessary).

On a system that has already booted, the following command can be issued to check if HPET is enabled::

   grep hpet /proc/timer_list

If no entries are returned, HPET must be enabled in the BIOS (as per the instructions above) and the system rebooted.

Linux Kernel Support
~~~~~~~~~~~~~~~~~~~~

The DPDK makes use of the platform HPET timer by mapping the timer counter into the process address space, and as such,
requires that the ``HPET_MMAP`` kernel configuration option be enabled.

.. warning::

    On Fedora, and other common distributions such as Ubuntu, the ``HPET_MMAP`` kernel option is not enabled by default.
    To recompile the Linux kernel with this option enabled, please consult the distributions documentation for the relevant instructions.

Enabling HPET in the DPDK
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, HPET support is disabled in the DPDK build configuration files.
To use HPET, use the following meson build option which will enable the HPET settings at compile time::

   meson configure -Duse_hpet=true

For an application to use the ``rte_get_hpet_cycles()`` and ``rte_get_hpet_hz()`` API calls,
and optionally to make the HPET the default time source for the rte_timer library,
the new ``rte_eal_hpet_init()`` API call should be called at application initialization.
This API call will ensure that the HPET is accessible, returning an error to the application if it is not,
for example, if ``HPET_MMAP`` is not enabled in the kernel.
The application can then determine what action to take, if any, if the HPET is not available at run-time.

.. note::

    For applications that require timing APIs, but not the HPET timer specifically,
    it is recommended that the ``rte_get_timer_cycles()`` and ``rte_get_timer_hz()`` API calls be used instead of the HPET-specific APIs.
    These generic APIs can work with either TSC or HPET time sources, depending on what is requested by an application call to ``rte_eal_hpet_init()``,
    if any, and on what is available on the system at runtime.

.. _Running_Without_Root_Privileges:

Running DPDK Applications Without Root Privileges
-------------------------------------------------

The following sections describe generic requirements and configuration
for running DPDK applications as non-root.
There may be additional requirements documented for some drivers.

Hugepages
~~~~~~~~~

Hugepages must be reserved as root before running the application as non-root,
for example::

  sudo dpdk-hugepages.py --reserve 1G

If multi-process is not required, running with ``--in-memory``
bypasses the need to access hugepage mount point and files within it.
Otherwise, hugepage directory must be made accessible
for writing to the unprivileged user.
A good way for managing multiple applications using hugepages
is to mount the filesystem with group permissions
and add a supplementary group to each application or container.

One option is to mount manually::

  mount -t hugetlbfs -o pagesize=1G,uid=`id -u`,gid=`id -g` nodev $HOME/huge-1G

In production environment, the OS can manage mount points
(`systemd example <https://github.com/systemd/systemd/blob/main/units/dev-hugepages.mount>`_).

The ``hugetlb`` filesystem has additional options to guarantee or limit
the amount of memory that is possible to allocate using the mount point.
Refer to the `documentation <https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt>`_.

.. note::

   Using ``vfio-pci`` kernel driver, if applicable, can eliminate the need
   for physical addresses and therefore eliminate the permission requirements
   described below.

If the driver requires using physical addresses (PA),
the executable file must be granted additional capabilities:

* ``SYS_ADMIN`` to read ``/proc/self/pagemaps``
* ``IPC_LOCK`` to lock hugepages in memory

.. code-block:: console

   setcap cap_ipc_lock,cap_sys_admin+ep <executable>

If physical addresses are not accessible,
the following message will appear during EAL initialization::

  EAL: rte_mem_virt2phy(): cannot open /proc/self/pagemap: Permission denied

It is harmless in case PA are not needed.

Resource Limits
~~~~~~~~~~~~~~~

When running as non-root user, there may be some additional resource limits
that are imposed by the system. Specifically, the following resource limits may
need to be adjusted in order to ensure normal DPDK operation:

* RLIMIT_LOCKS (number of file locks that can be held by a process)

* RLIMIT_NOFILE (number of open file descriptors that can be held open by a process)

* RLIMIT_MEMLOCK (amount of pinned pages the process is allowed to have)

The above limits can usually be adjusted by editing
``/etc/security/limits.conf`` file, and rebooting.

Device Control
~~~~~~~~~~~~~~

If the HPET is to be used, ``/dev/hpet`` permissions must be adjusted.

For ``vfio-pci`` kernel driver, the following Linux file system objects'
permissions should be adjusted:

* The VFIO device file, ``/dev/vfio/vfio``

* The directories under ``/dev/vfio`` that correspond to IOMMU group numbers of
  devices intended to be used by DPDK, for example, ``/dev/vfio/50``

Power Management and Power Saving Functionality
-----------------------------------------------

Enhanced Intel SpeedStep\ |reg| Technology must be enabled in the platform BIOS if the power management feature of DPDK is to be used.
Otherwise, the sys file folder ``/sys/devices/system/cpu/cpu0/cpufreq`` will not exist, and the CPU frequency- based power management cannot be used.
Consult the relevant BIOS documentation to determine how these settings can be accessed.

For example, on some Intel reference platform BIOS variants, the path to Enhanced Intel SpeedStep\ |reg| Technology is::

   Advanced
     -> Processor Configuration
     -> Enhanced Intel SpeedStep\ |reg| Tech

In addition, C3 and C6 should be enabled as well for power management. The path of C3 and C6 on the same platform BIOS is::

   Advanced
     -> Processor Configuration
     -> Processor C3 Advanced
     -> Processor Configuration
     -> Processor C6

Using Linux Core Isolation to Reduce Context Switches
-----------------------------------------------------

While the threads used by a DPDK application are pinned to logical cores on the system,
it is possible for the Linux scheduler to run other tasks on those cores also.
To help prevent additional workloads from running on those cores,
it is possible to use the ``isolcpus`` Linux kernel parameter to isolate them from the general Linux scheduler.

For example, if DPDK applications are to run on logical cores 2, 4 and 6,
the following should be added to the kernel parameter list:

.. code-block:: console

    isolcpus=2,4,6

Loading the DPDK KNI Kernel Module
----------------------------------

To run the DPDK Kernel NIC Interface (KNI) sample application, an extra kernel module (the kni module) must be loaded into the running kernel.
The module is found in the kernel/linux sub-directory of the DPDK build directory.
It should be loaded using the insmod command::

   insmod <build_dir>/kernel/linux/kni/rte_kni.ko

.. note::

   See the "Kernel NIC Interface Sample Application" chapter in the *DPDK Sample Applications User Guide* for more details.

Using Linux IOMMU Pass-Through to Run DPDK with Intel® VT-d
-----------------------------------------------------------

To enable Intel® VT-d in a Linux kernel, a number of kernel configuration options must be set. These include:

*   ``IOMMU_SUPPORT``

*   ``IOMMU_API``

*   ``INTEL_IOMMU``

In addition, to run the DPDK with Intel® VT-d, the ``iommu=pt`` kernel parameter must be used when using ``igb_uio`` driver.
This results in pass-through of the DMAR (DMA Remapping) lookup in the host.
Also, if ``INTEL_IOMMU_DEFAULT_ON`` is not set in the kernel, the ``intel_iommu=on`` kernel parameter must be used too.
This ensures that the Intel IOMMU is being initialized as expected.

Please note that while using ``iommu=pt`` is compulsory for ``igb_uio`` driver,
the ``vfio-pci`` driver can actually work with both ``iommu=pt`` and ``iommu=on``.
