..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Running Sample Applications
===========================

The chapter describes how to compile and run applications in a DPDK environment.
It also provides a pointer to where sample applications are stored.

Compiling a Sample Application
------------------------------

Please refer to :ref:`building_app_using_installed_dpdk` for detail on compiling sample apps.

Running a Sample Application
----------------------------

.. warning::

    Before running the application make sure:

    - Hugepages setup is done.
    - Any kernel driver being used is loaded.
    - In case needed, ports being used by the application should be
      bound to the corresponding kernel driver.

    refer to :ref:`linux_gsg_linux_drivers` for more details.

The application is linked with the DPDK target environment's Environmental Abstraction Layer (EAL) library,
which provides some options that are generic to every DPDK application.

The following is the list of options that can be given to the EAL:

.. code-block:: console

    ./rte-app [-l CORELIST] [-n NUM] [-b <domain:bus:devid.func>] \
              [--numa-mem=MB,...] [-d LIB.so|DIR] [-m MB] [-r NUM] [-v] [--file-prefix] \
	      [--proc-type <primary|secondary|auto>]

The EAL options are as follows:

* ``-l CORELIST``:
  A comma-separated list of the cores, or ranges of cores to run on.
  For example, ``-l 0,1,4-6`` will run on cores 0, 1, 4, 5 and 6.
  Note that core numbering can change between platforms and should be determined beforehand.

* ``-n NUM``:
  Number of memory channels per processor socket.

* ``-b <domain:bus:devid.func>``:
  Blocklisting of ports; prevent EAL from using specified PCI device
  (multiple ``-b`` options are allowed).

* ``--use-device``:
  use the specified Ethernet device(s) only. Use comma-separate
  ``[domain:]bus:devid.func`` values. Cannot be used with ``-b`` option.

* ``--numa-mem``:
  Memory to allocate from hugepages on specific sockets. In dynamic memory mode,
  this memory will also be pinned (i.e. not released back to the system until
  application closes).

* ``--numa-limit``:
  Limit maximum memory available for allocation on each socket. Does not support
  legacy memory mode.

* ``-d``:
  Add a driver or driver directory to be loaded.
  The application should use this option to load the PMDs
  that are built as shared libraries.

* ``-m MB``:
  Memory to allocate from hugepages, regardless of processor socket. It is
  recommended that ``--numa-mem`` be used instead of this option.

* ``-r NUM``:
  Number of memory ranks.

* ``-v``:
  Display version information on startup.

* ``--huge-dir``:
  The directory where hugetlbfs is mounted.

* ``--mbuf-pool-ops-name``:
  Pool ops name for mbuf to use.

* ``--file-prefix``:
  The prefix text used for hugepage filenames.

* ``--proc-type``:
  The type of process instance.

* ``--vmware-tsc-map``:
  Use VMware TSC map instead of native RDTSC.

* ``--base-virtaddr``:
  Specify base virtual address.

* ``--vfio-intr``:
  Specify interrupt type to be used by VFIO (has no effect if VFIO is not used).

* ``--legacy-mem``:
  Run DPDK in legacy memory mode (disable memory reserve/unreserve at runtime,
  but provide more IOVA-contiguous memory).

* ``--single-file-segments``:
  Store memory segments in fewer files (dynamic memory mode only - does not
  affect legacy memory mode).

Copy the DPDK application binary to your target, then run the application as follows
(assuming the platform has four memory channels per processor socket,
and that cores 0-3 are present and are to be used for running the application)::

    ./dpdk-helloworld -l 0-3 -n 4

.. note::

    The ``--proc-type`` and ``--file-prefix`` EAL options are used for running
    multiple DPDK processes. See the "Multi-process Sample Application"
    chapter in the *DPDK Sample Applications User Guide* and the *DPDK
    Programmers Guide* for more details.

Logical Core Use by Applications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The corelist (``-l``/``--lcores``) parameter is generally used for DPDK applications to specify the cores on which the application should run.
Since these logical core numbers, and their mapping to specific cores on specific NUMA sockets, can vary from platform to platform,
it is recommended that the core layout for each platform be considered when choosing the corelist to use in each case.

On initialization of the EAL layer by a DPDK application, the logical cores to be used and their socket location are displayed.
This information can also be determined for all cores on the system by examining the ``/proc/cpuinfo`` file, for example, by running cat ``/proc/cpuinfo``.
The physical id attribute listed for each processor indicates the CPU socket to which it belongs.
This can be useful when using other processors to understand the mapping of the logical cores to the sockets.

.. note::

   A more graphical view of the logical core layout
   may be obtained using the ``lstopo`` Linux utility.
   On Fedora, this may be installed and run using the following commands::

      sudo yum install hwloc
      lstopo

   This command produces a quite short textual output::

      lstopo-no-graphics --merge

.. warning::

    The logical core layout can change between different board layouts and should be checked before selecting an application corelist.

Hugepage Memory Use by Applications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When running an application, it is recommended to use the same amount of memory as that allocated for hugepages.
This is done automatically by the DPDK application at startup,
if no ``-m`` or ``--numa-mem`` parameter is passed to it when run.

If more memory is requested by explicitly passing a ``-m`` or ``--numa-mem`` value, the application fails.
However, the application itself can also fail if the user requests less memory than the reserved amount of hugepage-memory, particularly if using the ``-m`` option.
The reason is as follows.
Suppose the system has 1024 reserved 2 MB pages in socket 0 and 1024 in socket 1.
If the user requests 128 MB of memory, the 64 pages may not match the constraints:

*   The hugepage memory by be given to the application by the kernel in socket 1 only.
    In this case, if the application attempts to create an object, such as a ring or memory pool in socket 0, it fails.
    To avoid this issue, it is recommended that the ``--numa-mem`` option be used instead of the ``-m`` option.

*   These pages can be located anywhere in physical memory, and, although the DPDK EAL will attempt to allocate memory in contiguous blocks,
    it is possible that the pages will not be contiguous. In this case, the application is not able to allocate big memory pools.

The socket-mem option can be used to request specific amounts of memory for specific sockets.
This is accomplished by supplying the ``--numa-mem`` flag followed by amounts of memory requested on each socket,
for example, supply ``--numa-mem=0,512`` to try and reserve 512 MB for socket 1 only.
Similarly, on a four socket system, to allocate 1 GB memory on each of sockets 0 and 2 only, the parameter ``--numa-mem=1024,0,1024`` can be used.
No memory will be reserved on any CPU socket that is not explicitly referenced, for example, socket 3 in this case.
If the DPDK cannot allocate enough memory on each socket, the EAL initialization fails.

Whether hugepages are included in core dump is controlled by ``/proc/<pid>/coredump_filter``.
It is ``33`` (hexadecimal) by default, which means that hugepages are excluded from core dump.
This setting is per-process and is inherited.
Refer to ``core(5)`` for details.
To include mapped hugepages in core dump, set bit 6 (``0x40``) in the parent process
or shell before running a DPDK application:

.. code-block:: shell

   echo 0x73 > /proc/self/coredump_filter
   ./dpdk-application ...

.. note::

   Including hugepages in core dump file increases its size,
   which may fill the storage or overload the transport.
   Hugepages typically hold data processed by the application,
   like network packets, which may contain sensitive information.

Additional Sample Applications
------------------------------

Additional sample applications are included in the DPDK examples directory.
These sample applications may be built and run in a manner similar to that described in earlier sections in this manual.
In addition, see the *DPDK Sample Applications User Guide* for a description of the application,
specific instructions on compilation and execution and some explanation of the code.
