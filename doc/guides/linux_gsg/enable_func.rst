..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. include:: <isonum.txt>

Enabling Additional Functionality
=================================

.. _linux_gsg_running_without_root_privileges:

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

See :ref:`eal_hugepage_mapping` section to learn how these limits affect EAL.

Device Control
~~~~~~~~~~~~~~

For ``vfio-pci`` kernel driver, the following Linux file system objects'
permissions should be adjusted:

* The VFIO device file, ``/dev/vfio/vfio``

* The directories under ``/dev/vfio`` that correspond to IOMMU group numbers of
  devices intended to be used by DPDK, for example, ``/dev/vfio/50``

Power Management and Power Saving Functionality
-----------------------------------------------

Enhanced Intel SpeedStep\ |reg| Technology must be enabled in the platform BIOS if the power management feature of DPDK is to be used.
Otherwise, the sys file folder ``/sys/devices/system/cpu/cpu0/cpufreq`` will not exist, and the CPU frequency-based power management cannot be used.
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

.. note::

   It is not recommended to use CPU core 0 for DPDK polling applications
   because it cannot be truly isolated from other system and kernel activity.

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
