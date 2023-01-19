..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. include:: <isonum.txt>

.. _Enabling_Additional_Functionality:

Enabling Additional Functionality
=================================

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

One option is to use the script provided by this project::

  export HUGEDIR=$HOME/huge-1G
  mkdir -p $HUGEDIR
  sudo dpdk-hugepages.py --mount --directory $HUGEDIR --user `id -u` --group `id -g`

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

* ``DAC_READ_SEARCH`` and ``SYS_ADMIN`` to read ``/proc/self/pagemaps``
* ``IPC_LOCK`` to lock hugepages in memory

.. code-block:: console

   setcap cap_dac_read_search,cap_ipc_lock,cap_sys_admin+ep <executable>

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

See `Hugepage Mapping <hugepage_mapping>`_
section to learn how these limits affect EAL.

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
it is possible for the Linux scheduler to run other tasks on those cores.
To help prevent additional workloads, timers, RCU processing and IRQs
from running on those cores, it is possible to use
the Linux kernel parameters ``isolcpus``, ``nohz_full``, ``irqaffinity``
to isolate them from the general Linux scheduler tasks.

For example, if a given CPU has 0-7 cores
and DPDK applications are to run on logical cores 2, 4 and 6,
the following should be added to the kernel parameter list:

.. code-block:: console

   isolcpus=2,4,6 nohz_full=2,4,6 irqaffinity=0,1,3,5,7

.. note::

   More detailed information about the above parameters can be found at
   `NO_HZ <https://www.kernel.org/doc/html/latest/timers/no_hz.html>`_,
   `IRQ <https://www.kernel.org/doc/html/latest/core-api/irq/>`_,
   and `kernel parameters
   <https://www.kernel.org/doc/html/latest/admin-guide/kernel-parameters.html>`_

For more fine grained control over resource management and performance tuning
one can look into "Linux cgroups",
`cpusets <https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v1/cpusets.html>`_,
`cpuset man pages <https://man7.org/linux/man-pages/man7/cpuset.7.html>`_, and
`systemd CPU affinity <https://www.freedesktop.org/software/systemd/man/systemd.exec.html>`_.

Also see
`CPU isolation example <https://www.suse.com/c/cpu-isolation-practical-example-part-5/>`_
and `systemd core isolation example <https://www.rcannings.com/systemd-core-isolation/>`_.

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
