..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. include:: <isonum.txt>

.. _Enabling_Additional_Functionality:

Enabling Additional Functionality
=================================

.. _Running_Without_Root_Privileges:

Running DPDK Applications Without Root Privileges
-------------------------------------------------

In order to run DPDK as non-root, the following Linux filesystem objects'
permissions should be adjusted to ensure that the Linux account being used to
run the DPDK application has access to them:

*   All directories which serve as hugepage mount points, for example, ``/dev/hugepages``

*   If the HPET is to be used,  ``/dev/hpet``

When running as non-root user, there may be some additional resource limits
that are imposed by the system. Specifically, the following resource limits may
need to be adjusted in order to ensure normal DPDK operation:

* RLIMIT_LOCKS (number of file locks that can be held by a process)

* RLIMIT_NOFILE (number of open file descriptors that can be held open by a process)

* RLIMIT_MEMLOCK (amount of pinned pages the process is allowed to have)

The above limits can usually be adjusted by editing
``/etc/security/limits.conf`` file, and rebooting.

Additionally, depending on which kernel driver is in use, the relevant
resources also should be accessible by the user running the DPDK application.

For ``vfio-pci`` kernel driver, the following Linux file system objects'
permissions should be adjusted:

* The VFIO device file, ``/dev/vfio/vfio``

* The directories under ``/dev/vfio`` that correspond to IOMMU group numbers of
  devices intended to be used by DPDK, for example, ``/dev/vfio/50``

.. note::

    The instructions below will allow running DPDK with ``igb_uio`` or
    ``uio_pci_generic`` drivers as non-root with older Linux kernel versions.
    However, since version 4.0, the kernel does not allow unprivileged processes
    to read the physical address information from the pagemaps file, making it
    impossible for those processes to be used by non-privileged users. In such
    cases, using the VFIO driver is recommended.

For ``igb_uio`` or ``uio_pci_generic`` kernel drivers, the following Linux file
system objects' permissions should be adjusted:

*   The userspace-io device files in  ``/dev``, for example,  ``/dev/uio0``, ``/dev/uio1``, and so on

*   The userspace-io sysfs config and resource files, for example for ``uio0``::

       /sys/class/uio/uio0/device/config
       /sys/class/uio/uio0/device/resource*


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

.. _High_Precision_Event_Timer:

High Precision Event Timer (HPET) Functionality
-----------------------------------------------

DPDK can support the system HPET as a timer source rather than the system default timers,
such as the core Time-Stamp Counter (TSC) on x86 systems.
To enable HPET support in DPDK:

#. Ensure that HPET is enabled in BIOS settings.
#. Enable ``HPET_MMAP`` support in kernel configuration.
   Note that this my involve doing a kernel rebuild,
   as many common linux distributions do *not* have this setting
   enabled by default in their kernel builds.
#. Enable DPDK support for HPET by using the build-time meson option ``use_hpet``,
   for example, ``meson configure -Duse_hpet=true``

For an application to use the ``rte_get_hpet_cycles()`` and ``rte_get_hpet_hz()`` API calls,
and optionally to make the HPET the default time source for the rte_timer library,
the ``rte_eal_hpet_init()`` API call should be called at application initialization.
This API call will ensure that the HPET is accessible,
returning an error to the application if it is not.

For applications that require timing APIs, but not the HPET timer specifically,
it is recommended that the ``rte_get_timer_cycles()`` and ``rte_get_timer_hz()``
API calls be used instead of the HPET-specific APIs.
These generic APIs can work with either TSC or HPET time sources,
depending on what is requested by an application call to ``rte_eal_hpet_init()``,
if any, and on what is available on the system at runtime.
