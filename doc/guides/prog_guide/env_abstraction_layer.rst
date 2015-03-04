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

.. _Environment_Abstraction_Layer:

Environment Abstraction Layer
=============================

The Environment Abstraction Layer (EAL) is responsible for gaining access to low-level resources such as hardware and memory space.
It provides a generic interface that hides the environment specifics from the applications and libraries.
It is the responsibility of the initialization routine to decide how to allocate these resources
(that is, memory space, PCI devices, timers, consoles, and so on).

Typical services expected from the EAL are:

*   DPDK Loading and Launching:
    The DPDK and its application are linked as a single application and must be loaded by some means.

*   Core Affinity/Assignment Procedures:
    The EAL provides mechanisms for assigning execution units to specific cores as well as creating execution instances.

*   System Memory Reservation:
    The EAL facilitates the reservation of different memory zones, for example, physical memory areas for device interactions.

*   PCI Address Abstraction: The EAL provides an interface to access PCI address space.

*   Trace and Debug Functions: Logs, dump_stack, panic and so on.

*   Utility Functions: Spinlocks and atomic counters that are not provided in libc.

*   CPU Feature Identification: Determine at runtime if a particular feature, for example, Intel® AVX is supported.
    Determine if the current CPU supports the feature set that the binary was compiled for.

*   Interrupt Handling: Interfaces to register/unregister callbacks to specific interrupt sources.

*   Alarm Functions: Interfaces to set/remove callbacks to be run at a specific time.

EAL in a Linux-userland Execution Environment
---------------------------------------------

In a Linux user space environment, the DPDK application runs as a user-space application using the pthread library.
PCI information about devices and address space is discovered through the /sys kernel interface and through kernel modules such as uio_pci_generic, or igb_uio.
Refer to the UIO: User-space drivers documentation in the Linux kernel. This memory is mmap'd in the application.

The EAL performs physical memory allocation using mmap() in hugetlbfs (using huge page sizes to increase performance).
This memory is exposed to DPDK service layers such as the :ref:`Mempool Library <Mempool_Library>`.

At this point, the DPDK services layer will be initialized, then through pthread setaffinity calls,
each execution unit will be assigned to a specific logical core to run as a user-level thread.

The time reference is provided by the CPU Time-Stamp Counter (TSC) or by the HPET kernel API through a mmap() call.

Initialization and Core Launching
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Part of the initialization is done by the start function of glibc.
A check is also performed at initialization time to ensure that the micro architecture type chosen in the config file is supported by the CPU.
Then, the main() function is called. The core initialization and launch is done in rte_eal_init() (see the API documentation).
It consist of calls to the pthread library (more specifically, pthread_self(), pthread_create(), and pthread_setaffinity_np()).

.. _pg_figure_2:

**Figure 2. EAL Initialization in a Linux Application Environment**

.. image3_png has been replaced

|linuxapp_launch|

.. note::

    Initialization of objects, such as memory zones, rings, memory pools, lpm tables and hash tables,
    should be done as part of the overall application initialization on the master lcore.
    The creation and initialization functions for these objects are not multi-thread safe.
    However, once initialized, the objects themselves can safely be used in multiple threads simultaneously.

Multi-process Support
~~~~~~~~~~~~~~~~~~~~~

The Linuxapp EAL allows a multi-process as well as a multi-threaded (pthread) deployment model.
See chapter 2.20
:ref:`Multi-process Support <Multi-process_Support>` for more details.

Memory Mapping Discovery and Memory Reservation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The allocation of large contiguous physical memory is done using the hugetlbfs kernel filesystem.
The EAL provides an API to reserve named memory zones in this contiguous memory.
The physical address of the reserved memory for that memory zone is also returned to the user by the memory zone reservation API.

.. note::

    Memory reservations done using the APIs provided by the rte_malloc library are also backed by pages from the hugetlbfs filesystem.
    However, physical address information is not available for the blocks of memory allocated in this way.

Xen Dom0 support without hugetbls
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The existing memory management implementation is based on the Linux kernel hugepage mechanism.
However, Xen Dom0 does not support hugepages, so a new Linux kernel module rte_dom0_mm is added to workaround this limitation.

The EAL uses IOCTL interface to notify the Linux kernel module rte_dom0_mm to allocate memory of specified size,
and get all memory segments information from the module,
and the EAL uses MMAP interface to map the allocated memory.
For each memory segment, the physical addresses are contiguous within it but actual hardware addresses are contiguous within 2MB.

PCI Access
~~~~~~~~~~

The EAL uses the /sys/bus/pci utilities provided by the kernel to scan the content on the PCI bus.
To access PCI memory, a kernel module called uio_pci_generic provides a /dev/uioX device file
and resource files in /sys
that can be mmap'd to obtain access to PCI address space from the application.
The DPDK-specific igb_uio module can also be used for this. Both drivers use the uio kernel feature (userland driver).

Per-lcore and Shared Variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

    lcore refers to a logical execution unit of the processor, sometimes called a hardware *thread*.

Shared variables are the default behavior.
Per-lcore variables are implemented using *Thread Local Storage* (TLS) to provide per-thread local storage.

Logs
~~~~

A logging API is provided by EAL.
By default, in a Linux application, logs are sent to syslog and also to the console.
However, the log function can be overridden by the user to use a different logging mechanism.

Trace and Debug Functions
^^^^^^^^^^^^^^^^^^^^^^^^^

There are some debug functions to dump the stack in glibc.
The rte_panic() function can voluntarily provoke a SIG_ABORT,
which can trigger the generation of a core file, readable by gdb.

CPU Feature Identification
~~~~~~~~~~~~~~~~~~~~~~~~~~

The EAL can query the CPU at runtime (using the rte_cpu_get_feature() function) to determine which CPU features are available.

User Space Interrupt and Alarm Handling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The EAL creates a host thread to poll the UIO device file descriptors to detect the interrupts.
Callbacks can be registered or unregistered by the EAL functions for a specific interrupt event
and are called in the host thread asynchronously.
The EAL also allows timed callbacks to be used in the same way as for NIC interrupts.

.. note::

    The only interrupts supported by the DPDK Poll-Mode Drivers are those for link status change,
    i.e. link up and link down notification.

Blacklisting
~~~~~~~~~~~~

The EAL PCI device blacklist functionality can be used to mark certain NIC ports as blacklisted,
so they are ignored by the DPDK.
The ports to be blacklisted are identified using the PCIe* description (Domain:Bus:Device.Function).

Misc Functions
~~~~~~~~~~~~~~

Locks and atomic operations are per-architecture (i686 and x86_64).

Memory Segments and Memory Zones (memzone)
------------------------------------------

The mapping of physical memory is provided by this feature in the EAL.
As physical memory can have gaps, the memory is described in a table of descriptors,
and each descriptor (called rte_memseg ) describes a contiguous portion of memory.

On top of this, the memzone allocator's role is to reserve contiguous portions of physical memory.
These zones are identified by a unique name when the memory is reserved.

The rte_memzone descriptors are also located in the configuration structure.
This structure is accessed using rte_eal_get_configuration().
The lookup (by name) of a memory zone returns a descriptor containing the physical address of the memory zone.

Memory zones can be reserved with specific start address alignment by supplying the align parameter
(by default, they are aligned to cache line size).
The alignment value should be a power of two and not less than the cache line size (64 bytes).
Memory zones can also be reserved from either 2 MB or 1 GB hugepages, provided that both are available on the system.


Multiple pthread
----------------

DPDK usually pins one pthread per core to avoid the overhead of task switching.
This allows for significant performance gains, but lacks flexibility and is not always efficient.

Power management helps to improve the CPU efficiency by limiting the CPU runtime frequency.
However, alternately it is possible to utilize the idle cycles available to take advantage of
the full capability of the CPU.

By taking advantage of cgroup, the CPU utilization quota can be simply assigned.
This gives another way to improve the CPU efficienct, however, there is a prerequisite;
DPDK must handle the context switching between multiple pthreads per core.

For further flexibility, it is useful to set pthread affinity not only to a CPU but to a CPU set.

EAL pthread and lcore Affinity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The term "lcore" refers to an EAL thread, which is really a Linux/FreeBSD pthread.
"EAL pthreads"  are created and managed by EAL and execute the tasks issued by *remote_launch*.
In each EAL pthread, there is a TLS (Thread Local Storage) called *_lcore_id* for unique identification.
As EAL pthreads usually bind 1:1 to the physical CPU, the *_lcore_id* is typically equal to the CPU ID.

When using multiple pthreads, however, the binding is no longer always 1:1 between an EAL pthread and a specified physical CPU.
The EAL pthread may have affinity to a CPU set, and as such the *_lcore_id* will not be the same as the CPU ID.
For this reason, there is an EAL long option '--lcores' defined to assign the CPU affinity of lcores.
For a specified lcore ID or ID group, the option allows setting the CPU set for that EAL pthread.

The format pattern:
	--lcores='<lcore_set>[@cpu_set][,<lcore_set>[@cpu_set],...]'

'lcore_set' and 'cpu_set' can be a single number, range or a group.

A number is a "digit([0-9]+)"; a range is "<number>-<number>"; a group is "(<number|range>[,<number|range>,...])".

If a '\@cpu_set' value is not supplied, the value of 'cpu_set' will default to the value of 'lcore_set'.

    ::

    	For example, "--lcores='1,2@(5-7),(3-5)@(0,2),(0,6),7-8'" which means start 9 EAL thread;
    	    lcore 0 runs on cpuset 0x41 (cpu 0,6);
    	    lcore 1 runs on cpuset 0x2 (cpu 1);
    	    lcore 2 runs on cpuset 0xe0 (cpu 5,6,7);
    	    lcore 3,4,5 runs on cpuset 0x5 (cpu 0,2);
    	    lcore 6 runs on cpuset 0x41 (cpu 0,6);
    	    lcore 7 runs on cpuset 0x80 (cpu 7);
    	    lcore 8 runs on cpuset 0x100 (cpu 8).

Using this option, for each given lcore ID, the associated CPUs can be assigned.
It's also compatible with the pattern of corelist('-l') option.

non-EAL pthread support
~~~~~~~~~~~~~~~~~~~~~~~

It is possible to use the DPDK execution context with any user pthread (aka. Non-EAL pthreads).
In a non-EAL pthread, the *_lcore_id* is always LCORE_ID_ANY which identifies that it is not an EAL thread with a valid, unique, *_lcore_id*.
Some libraries will use an alternative unique ID (e.g. TID), some will not be impacted at all, and some will work but with limitations (e.g. timer and mempool libraries).

All these impacts are mentioned in :ref:`known_issue_label` section.

Public Thread API
~~~~~~~~~~~~~~~~~

There are two public APIs ``rte_thread_set_affinity()`` and ``rte_pthread_get_affinity()`` introduced for threads.
When they're used in any pthread context, the Thread Local Storage(TLS) will be set/get.

Those TLS include *_cpuset* and *_socket_id*:

*	*_cpuset* stores the CPUs bitmap to which the pthread is affinitized.

*	*_socket_id* stores the NUMA node of the CPU set. If the CPUs in CPU set belong to different NUMA node, the *_socket_id* will be set to SOCKTE_ID_ANY.


.. _known_issue_label:

Known Issues
~~~~~~~~~~~~

+ rte_mempool

  The rte_mempool uses a per-lcore cache inside the mempool.
  For non-EAL pthreads, ``rte_lcore_id()`` will not return a valid number.
  So for now, when rte_mempool is used with non-EAL pthreads, the put/get operations will bypass the mempool cache and there is a performance penalty because of this bypass.
  Support for non-EAL mempool cache is currently being enabled.

+ rte_ring

  rte_ring supports multi-producer enqueue and multi-consumer dequeue.
  However, it is non-preemptive, this has a knock on effect of making rte_mempool non-preemtable.

  .. note::

    The "non-preemptive" constraint means:

    - a pthread doing multi-producers enqueues on a given ring must not
      be preempted by another pthread doing a multi-producer enqueue on
      the same ring.
    - a pthread doing multi-consumers dequeues on a given ring must not
      be preempted by another pthread doing a multi-consumer dequeue on
      the same ring.

    Bypassing this constraint it may cause the 2nd pthread to spin until the 1st one is scheduled again.
    Moreover, if the 1st pthread is preempted by a context that has an higher priority, it may even cause a dead lock.

  This does not mean it cannot be used, simply, there is a need to narrow down the situation when it is used by multi-pthread on the same core.

  1. It CAN be used for any single-producer or single-consumer situation.

  2. It MAY be used by multi-producer/consumer pthread whose scheduling policy are all SCHED_OTHER(cfs). User SHOULD be aware of the performance penalty before using it.

  3. It MUST not be used by multi-producer/consumer pthreads, whose scheduling policies are SCHED_FIFO or SCHED_RR.

  ``RTE_RING_PAUSE_REP_COUNT`` is defined for rte_ring to reduce contention. It's mainly for case 2, a yield is issued after number of times pause repeat.

  It adds a sched_yield() syscall if the thread spins for too long while waiting on the other thread to finish its operations on the ring.
  This gives the pre-empted thread a chance to proceed and finish with the ring enqueue/dequeue operation.

+ rte_timer

  Running  ``rte_timer_manager()`` on a non-EAL pthread is not allowed. However, resetting/stopping the timer from a non-EAL pthread is allowed.

+ rte_log

  In non-EAL pthreads, there is no per thread loglevel and logtype, global loglevels are used.

+ misc

  The debug statistics of rte_ring, rte_mempool and rte_timer are not supported in a non-EAL pthread.

cgroup control
~~~~~~~~~~~~~~

The following is a simple example of cgroup control usage, there are two pthreads(t0 and t1) doing packet I/O on the same core ($CPU).
We expect only 50% of CPU spend on packet IO.

  .. code::

    mkdir /sys/fs/cgroup/cpu/pkt_io
    mkdir /sys/fs/cgroup/cpuset/pkt_io

    echo $cpu > /sys/fs/cgroup/cpuset/cpuset.cpus

    echo $t0 > /sys/fs/cgroup/cpu/pkt_io/tasks
    echo $t0 > /sys/fs/cgroup/cpuset/pkt_io/tasks

    echo $t1 > /sys/fs/cgroup/cpu/pkt_io/tasks
    echo $t1 > /sys/fs/cgroup/cpuset/pkt_io/tasks

    cd /sys/fs/cgroup/cpu/pkt_io
    echo 100000 > pkt_io/cpu.cfs_period_us
    echo  50000 > pkt_io/cpu.cfs_quota_us


.. |linuxapp_launch| image:: img/linuxapp_launch.*
