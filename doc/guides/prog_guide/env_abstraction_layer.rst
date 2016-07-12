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

.. _figure_linuxapp_launch:

.. figure:: img/linuxapp_launch.*

   EAL Initialization in a Linux Application Environment


.. note::

    Initialization of objects, such as memory zones, rings, memory pools, lpm tables and hash tables,
    should be done as part of the overall application initialization on the master lcore.
    The creation and initialization functions for these objects are not multi-thread safe.
    However, once initialized, the objects themselves can safely be used in multiple threads simultaneously.

Multi-process Support
~~~~~~~~~~~~~~~~~~~~~

The Linuxapp EAL allows a multi-process as well as a multi-threaded (pthread) deployment model.
See chapter
:ref:`Multi-process Support <Multi-process_Support>` for more details.

Memory Mapping Discovery and Memory Reservation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The allocation of large contiguous physical memory is done using the hugetlbfs kernel filesystem.
The EAL provides an API to reserve named memory zones in this contiguous memory.
The physical address of the reserved memory for that memory zone is also returned to the user by the memory zone reservation API.

.. note::

    Memory reservations done using the APIs provided by rte_malloc are also backed by pages from the hugetlbfs filesystem.

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

User Space Interrupt Event
~~~~~~~~~~~~~~~~~~~~~~~~~~

+ User Space Interrupt and Alarm Handling in Host Thread

The EAL creates a host thread to poll the UIO device file descriptors to detect the interrupts.
Callbacks can be registered or unregistered by the EAL functions for a specific interrupt event
and are called in the host thread asynchronously.
The EAL also allows timed callbacks to be used in the same way as for NIC interrupts.

.. note::

    In DPDK PMD, the only interrupts handled by the dedicated host thread are those for link status change,
    i.e. link up and link down notification.


+ RX Interrupt Event

The receive and transmit routines provided by each PMD don't limit themselves to execute in polling thread mode.
To ease the idle polling with tiny throughput, it's useful to pause the polling and wait until the wake-up event happens.
The RX interrupt is the first choice to be such kind of wake-up event, but probably won't be the only one.

EAL provides the event APIs for this event-driven thread mode.
Taking linuxapp as an example, the implementation relies on epoll. Each thread can monitor an epoll instance
in which all the wake-up events' file descriptors are added. The event file descriptors are created and mapped to
the interrupt vectors according to the UIO/VFIO spec.
From bsdapp's perspective, kqueue is the alternative way, but not implemented yet.

EAL initializes the mapping between event file descriptors and interrupt vectors, while each device initializes the mapping
between interrupt vectors and queues. In this way, EAL actually is unaware of the interrupt cause on the specific vector.
The eth_dev driver takes responsibility to program the latter mapping.

.. note::

    Per queue RX interrupt event is only allowed in VFIO which supports multiple MSI-X vector. In UIO, the RX interrupt
    together with other interrupt causes shares the same vector. In this case, when RX interrupt and LSC(link status change)
    interrupt are both enabled(intr_conf.lsc == 1 && intr_conf.rxq == 1), only the former is capable.

The RX interrupt are controlled/enabled/disabled by ethdev APIs - 'rte_eth_dev_rx_intr_*'. They return failure if the PMD
hasn't support them yet. The intr_conf.rxq flag is used to turn on the capability of RX interrupt per device.

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
This gives another way to improve the CPU efficiency, however, there is a prerequisite;
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

*	*_socket_id* stores the NUMA node of the CPU set. If the CPUs in CPU set belong to different NUMA node, the *_socket_id* will be set to SOCKET_ID_ANY.


.. _known_issue_label:

Known Issues
~~~~~~~~~~~~

+ rte_mempool

  The rte_mempool uses a per-lcore cache inside the mempool.
  For non-EAL pthreads, ``rte_lcore_id()`` will not return a valid number.
  So for now, when rte_mempool is used with non-EAL pthreads, the put/get operations will bypass the default mempool cache and there is a performance penalty because of this bypass.
  Only user-owned external caches can be used in a non-EAL context in conjunction with ``rte_mempool_generic_put()`` and ``rte_mempool_generic_get()`` that accept an explicit cache parameter.

+ rte_ring

  rte_ring supports multi-producer enqueue and multi-consumer dequeue.
  However, it is non-preemptive, this has a knock on effect of making rte_mempool non-preemptable.

  .. note::

    The "non-preemptive" constraint means:

    - a pthread doing multi-producers enqueues on a given ring must not
      be preempted by another pthread doing a multi-producer enqueue on
      the same ring.
    - a pthread doing multi-consumers dequeues on a given ring must not
      be preempted by another pthread doing a multi-consumer dequeue on
      the same ring.

    Bypassing this constraint may cause the 2nd pthread to spin until the 1st one is scheduled again.
    Moreover, if the 1st pthread is preempted by a context that has an higher priority, it may even cause a dead lock.

  This does not mean it cannot be used, simply, there is a need to narrow down the situation when it is used by multi-pthread on the same core.

  1. It CAN be used for any single-producer or single-consumer situation.

  2. It MAY be used by multi-producer/consumer pthread whose scheduling policy are all SCHED_OTHER(cfs). User SHOULD be aware of the performance penalty before using it.

  3. It MUST not be used by multi-producer/consumer pthreads, whose scheduling policies are SCHED_FIFO or SCHED_RR.

  ``RTE_RING_PAUSE_REP_COUNT`` is defined for rte_ring to reduce contention. It's mainly for case 2, a yield is issued after number of times pause repeat.

  It adds a sched_yield() syscall if the thread spins for too long while waiting on the other thread to finish its operations on the ring.
  This gives the preempted thread a chance to proceed and finish with the ring enqueue/dequeue operation.

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

  .. code-block:: console

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


Malloc
------

The EAL provides a malloc API to allocate any-sized memory.

The objective of this API is to provide malloc-like functions to allow
allocation from hugepage memory and to facilitate application porting.
The *DPDK API Reference* manual describes the available functions.

Typically, these kinds of allocations should not be done in data plane
processing because they are slower than pool-based allocation and make
use of locks within the allocation and free paths.
However, they can be used in configuration code.

Refer to the rte_malloc() function description in the *DPDK API Reference*
manual for more information.

Cookies
~~~~~~~

When CONFIG_RTE_MALLOC_DEBUG is enabled, the allocated memory contains
overwrite protection fields to help identify buffer overflows.

Alignment and NUMA Constraints
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The rte_malloc() takes an align argument that can be used to request a memory
area that is aligned on a multiple of this value (which must be a power of two).

On systems with NUMA support, a call to the rte_malloc() function will return
memory that has been allocated on the NUMA socket of the core which made the call.
A set of APIs is also provided, to allow memory to be explicitly allocated on a
NUMA socket directly, or by allocated on the NUMA socket where another core is
located, in the case where the memory is to be used by a logical core other than
on the one doing the memory allocation.

Use Cases
~~~~~~~~~

This API is meant to be used by an application that requires malloc-like
functions at initialization time.

For allocating/freeing data at runtime, in the fast-path of an application,
the memory pool library should be used instead.

Internal Implementation
~~~~~~~~~~~~~~~~~~~~~~~

Data Structures
^^^^^^^^^^^^^^^

There are two data structure types used internally in the malloc library:

*   struct malloc_heap - used to track free space on a per-socket basis

*   struct malloc_elem - the basic element of allocation and free-space
    tracking inside the library.

Structure: malloc_heap
""""""""""""""""""""""

The malloc_heap structure is used to manage free space on a per-socket basis.
Internally, there is one heap structure per NUMA node, which allows us to
allocate memory to a thread based on the NUMA node on which this thread runs.
While this does not guarantee that the memory will be used on that NUMA node,
it is no worse than a scheme where the memory is always allocated on a fixed
or random node.

The key fields of the heap structure and their function are described below
(see also diagram above):

*   lock - the lock field is needed to synchronize access to the heap.
    Given that the free space in the heap is tracked using a linked list,
    we need a lock to prevent two threads manipulating the list at the same time.

*   free_head - this points to the first element in the list of free nodes for
    this malloc heap.

.. note::

    The malloc_heap structure does not keep track of in-use blocks of memory,
    since these are never touched except when they are to be freed again -
    at which point the pointer to the block is an input to the free() function.

.. _figure_malloc_heap:

.. figure:: img/malloc_heap.*

   Example of a malloc heap and malloc elements within the malloc library


.. _malloc_elem:

Structure: malloc_elem
""""""""""""""""""""""

The malloc_elem structure is used as a generic header structure for various
blocks of memory.
It is used in three different ways - all shown in the diagram above:

#.  As a header on a block of free or allocated memory - normal case

#.  As a padding header inside a block of memory

#.  As an end-of-memseg marker

The most important fields in the structure and how they are used are described below.

.. note::

    If the usage of a particular field in one of the above three usages is not
    described, the field can be assumed to have an undefined value in that
    situation, for example, for padding headers only the "state" and "pad"
    fields have valid values.

*   heap - this pointer is a reference back to the heap structure from which
    this block was allocated.
    It is used for normal memory blocks when they are being freed, to add the
    newly-freed block to the heap's free-list.

*   prev - this pointer points to the header element/block in the memseg
    immediately behind the current one. When freeing a block, this pointer is
    used to reference the previous block to check if that block is also free.
    If so, then the two free blocks are merged to form a single larger block.

*   next_free - this pointer is used to chain the free-list of unallocated
    memory blocks together.
    It is only used in normal memory blocks; on ``malloc()`` to find a suitable
    free block to allocate and on ``free()`` to add the newly freed element to
    the free-list.

*   state - This field can have one of three values: ``FREE``, ``BUSY`` or
    ``PAD``.
    The former two are to indicate the allocation state of a normal memory block
    and the latter is to indicate that the element structure is a dummy structure
    at the end of the start-of-block padding, i.e. where the start of the data
    within a block is not at the start of the block itself, due to alignment
    constraints.
    In that case, the pad header is used to locate the actual malloc element
    header for the block.
    For the end-of-memseg structure, this is always a ``BUSY`` value, which
    ensures that no element, on being freed, searches beyond the end of the
    memseg for other blocks to merge with into a larger free area.

*   pad - this holds the length of the padding present at the start of the block.
    In the case of a normal block header, it is added to the address of the end
    of the header to give the address of the start of the data area, i.e. the
    value passed back to the application on a malloc.
    Within a dummy header inside the padding, this same value is stored, and is
    subtracted from the address of the dummy header to yield the address of the
    actual block header.

*   size - the size of the data block, including the header itself.
    For end-of-memseg structures, this size is given as zero, though it is never
    actually checked.
    For normal blocks which are being freed, this size value is used in place of
    a "next" pointer to identify the location of the next block of memory that
    in the case of being ``FREE``, the two free blocks can be merged into one.

Memory Allocation
^^^^^^^^^^^^^^^^^

On EAL initialization, all memsegs are setup as part of the malloc heap.
This setup involves placing a dummy structure at the end with ``BUSY`` state,
which may contain a sentinel value if ``CONFIG_RTE_MALLOC_DEBUG`` is enabled,
and a proper :ref:`element header<malloc_elem>` with ``FREE`` at the start
for each memseg.
The ``FREE`` element is then added to the ``free_list`` for the malloc heap.

When an application makes a call to a malloc-like function, the malloc function
will first index the ``lcore_config`` structure for the calling thread, and
determine the NUMA node of that thread.
The NUMA node is used to index the array of ``malloc_heap`` structures which is
passed as a parameter to the ``heap_alloc()`` function, along with the
requested size, type, alignment and boundary parameters.

The ``heap_alloc()`` function will scan the free_list of the heap, and attempt
to find a free block suitable for storing data of the requested size, with the
requested alignment and boundary constraints.

When a suitable free element has been identified, the pointer to be returned
to the user is calculated.
The cache-line of memory immediately preceding this pointer is filled with a
struct malloc_elem header.
Because of alignment and boundary constraints, there could be free space at
the start and/or end of the element, resulting in the following behavior:

#. Check for trailing space.
   If the trailing space is big enough, i.e. > 128 bytes, then the free element
   is split.
   If it is not, then we just ignore it (wasted space).

#. Check for space at the start of the element.
   If the space at the start is small, i.e. <=128 bytes, then a pad header is
   used, and the remaining space is wasted.
   If, however, the remaining space is greater, then the free element is split.

The advantage of allocating the memory from the end of the existing element is
that no adjustment of the free list needs to take place - the existing element
on the free list just has its size pointer adjusted, and the following element
has its "prev" pointer redirected to the newly created element.

Freeing Memory
^^^^^^^^^^^^^^

To free an area of memory, the pointer to the start of the data area is passed
to the free function.
The size of the ``malloc_elem`` structure is subtracted from this pointer to get
the element header for the block.
If this header is of type ``PAD`` then the pad length is further subtracted from
the pointer to get the proper element header for the entire block.

From this element header, we get pointers to the heap from which the block was
allocated and to where it must be freed, as well as the pointer to the previous
element, and via the size field, we can calculate the pointer to the next element.
These next and previous elements are then checked to see if they are also
``FREE``, and if so, they are merged with the current element.
This means that we can never have two ``FREE`` memory blocks adjacent to one
another, as they are always merged into a single block.
