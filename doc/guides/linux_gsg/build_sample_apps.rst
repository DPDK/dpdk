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

    ./rte-app [-c COREMASK | -l CORELIST] [-n NUM] [-b <domain:bus:devid.func>] \
              [--socket-mem=MB,...] [-d LIB.so|DIR] [-m MB] [-r NUM] [-v] [--file-prefix] \
	      [--proc-type <primary|secondary|auto>]

The EAL options are as follows:

* ``-c COREMASK`` or ``-l CORELIST``:
  An hexadecimal bit mask of the cores to run on. Note that core numbering can
  change between platforms and should be determined beforehand. The corelist is
  a set of core numbers instead of a bitmap core mask.

* ``-n NUM``:
  Number of memory channels per processor socket.

* ``-b <domain:bus:devid.func>``:
  Blocklisting of ports; prevent EAL from using specified PCI device
  (multiple ``-b`` options are allowed).

* ``--use-device``:
  use the specified Ethernet device(s) only. Use comma-separate
  ``[domain:]bus:devid.func`` values. Cannot be used with ``-b`` option.

* ``--socket-mem``:
  Memory to allocate from hugepages on specific sockets. In dynamic memory mode,
  this memory will also be pinned (i.e. not released back to the system until
  application closes).

* ``--socket-limit``:
  Limit maximum memory available for allocation on each socket. Does not support
  legacy memory mode.

* ``-d``:
  Add a driver or driver directory to be loaded.
  The application should use this option to load the pmd drivers
  that are built as shared libraries.

* ``-m MB``:
  Memory to allocate from hugepages, regardless of processor socket. It is
  recommended that ``--socket-mem`` be used instead of this option.

* ``-r NUM``:
  Number of memory ranks.

* ``-v``:
  Display version information on startup.

* ``--huge-dir``:
  The directory where hugetlbfs is mounted.

* ``mbuf-pool-ops-name``:
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

The ``-c`` or ``-l`` and option is mandatory; the others are optional.

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

The coremask (-c 0x0f) or corelist (-l 0-3) parameter is always mandatory for DPDK applications.
Each bit of the mask corresponds to the equivalent logical core number as reported by Linux. The preferred corelist option is a cleaner method to define cores to be used.
Since these logical core numbers, and their mapping to specific cores on specific NUMA sockets, can vary from platform to platform,
it is recommended that the core layout for each platform be considered when choosing the coremask/corelist to use in each case.

On initialization of the EAL layer by a DPDK application, the logical cores to be used and their socket location are displayed.
This information can also be determined for all cores on the system by examining the ``/proc/cpuinfo`` file, for example, by running cat ``/proc/cpuinfo``.
The physical id attribute listed for each processor indicates the CPU socket to which it belongs.
This can be useful when using other processors to understand the mapping of the logical cores to the sockets.

.. note::

    A more graphical view of the logical core layout may be obtained using the ``lstopo`` Linux utility.
    On Fedora Linux, this may be installed and run using the following command::

        sudo yum install hwloc
        ./lstopo

.. warning::

    The logical core layout can change between different board layouts and should be checked before selecting an application coremask/corelist.

Hugepage Memory Use by Applications
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When running an application, it is recommended to use the same amount of memory as that allocated for hugepages.
This is done automatically by the DPDK application at startup,
if no ``-m`` or ``--socket-mem`` parameter is passed to it when run.

If more memory is requested by explicitly passing a ``-m`` or ``--socket-mem`` value, the application fails.
However, the application itself can also fail if the user requests less memory than the reserved amount of hugepage-memory, particularly if using the ``-m`` option.
The reason is as follows.
Suppose the system has 1024 reserved 2 MB pages in socket 0 and 1024 in socket 1.
If the user requests 128 MB of memory, the 64 pages may not match the constraints:

*   The hugepage memory by be given to the application by the kernel in socket 1 only.
    In this case, if the application attempts to create an object, such as a ring or memory pool in socket 0, it fails.
    To avoid this issue, it is recommended that the ``--socket-mem`` option be used instead of the ``-m`` option.

*   These pages can be located anywhere in physical memory, and, although the DPDK EAL will attempt to allocate memory in contiguous blocks,
    it is possible that the pages will not be contiguous. In this case, the application is not able to allocate big memory pools.

The socket-mem option can be used to request specific amounts of memory for specific sockets.
This is accomplished by supplying the ``--socket-mem`` flag followed by amounts of memory requested on each socket,
for example, supply ``--socket-mem=0,512`` to try and reserve 512 MB for socket 1 only.
Similarly, on a four socket system, to allocate 1 GB memory on each of sockets 0 and 2 only, the parameter ``--socket-mem=1024,0,1024`` can be used.
No memory will be reserved on any CPU socket that is not explicitly referenced, for example, socket 3 in this case.
If the DPDK cannot allocate enough memory on each socket, the EAL initialization fails.

Additional Sample Applications
------------------------------

Additional sample applications are included in the DPDK examples directory.
These sample applications may be built and run in a manner similar to that described in earlier sections in this manual.
In addition, see the *DPDK Sample Applications User Guide* for a description of the application,
specific instructions on compilation and execution and some explanation of the code.
