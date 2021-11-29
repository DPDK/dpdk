..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Environment_Abstraction_Layer:

Environment Abstraction Layer
=============================

The Environment Abstraction Layer (EAL) is responsible for gaining access to low-level resources such as hardware and memory space.
It provides a generic interface that hides the environment specifics from the applications and libraries.
It is the responsibility of the initialization routine to decide how to allocate these resources
(that is, memory space, devices, timers, consoles, and so on).

Typical services expected from the EAL are:

*   DPDK Loading and Launching:
    The DPDK and its application are linked as a single application and must be loaded by some means.

*   Core Affinity/Assignment Procedures:
    The EAL provides mechanisms for assigning execution units to specific cores as well as creating execution instances.

*   System Memory Reservation:
    The EAL facilitates the reservation of different memory zones, for example, physical memory areas for device interactions.

*   Trace and Debug Functions: Logs, dump_stack, panic and so on.

*   Utility Functions: Spinlocks and atomic counters that are not provided in libc.

*   CPU Feature Identification: Determine at runtime if a particular feature, for example, Intel® AVX is supported.
    Determine if the current CPU supports the feature set that the binary was compiled for.

*   Interrupt Handling: Interfaces to register/unregister callbacks to specific interrupt sources.

*   Alarm Functions: Interfaces to set/remove callbacks to be run at a specific time.

EAL in a Linux-userland Execution Environment
---------------------------------------------

In a Linux user space environment, the DPDK application runs as a user-space application using the pthread library.

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

.. _figure_linux_launch:

.. figure:: img/linuxapp_launch.*

   EAL Initialization in a Linux Application Environment


.. note::

    Initialization of objects, such as memory zones, rings, memory pools, lpm tables and hash tables,
    should be done as part of the overall application initialization on the main lcore.
    The creation and initialization functions for these objects are not multi-thread safe.
    However, once initialized, the objects themselves can safely be used in multiple threads simultaneously.

Shutdown and Cleanup
~~~~~~~~~~~~~~~~~~~~

During the initialization of EAL resources such as hugepage backed memory can be
allocated by core components.  The memory allocated during ``rte_eal_init()``
can be released by calling the ``rte_eal_cleanup()`` function. Refer to the
API documentation for details.

Multi-process Support
~~~~~~~~~~~~~~~~~~~~~

The Linux EAL allows a multi-process as well as a multi-threaded (pthread) deployment model.
See chapter
:ref:`Multi-process Support <Multi-process_Support>` for more details.

Memory Mapping Discovery and Memory Reservation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The allocation of large contiguous physical memory is done using the hugetlbfs kernel filesystem.
The EAL provides an API to reserve named memory zones in this contiguous memory.
The physical address of the reserved memory for that memory zone is also returned to the user by the memory zone reservation API.

There are two modes in which DPDK memory subsystem can operate: dynamic mode,
and legacy mode. Both modes are explained below.

.. note::

    Memory reservations done using the APIs provided by rte_malloc are also backed by pages from the hugetlbfs filesystem.

+ Dynamic memory mode

Currently, this mode is only supported on Linux.

In this mode, usage of hugepages by DPDK application will grow and shrink based
on application's requests. Any memory allocation through ``rte_malloc()``,
``rte_memzone_reserve()`` or other methods, can potentially result in more
hugepages being reserved from the system. Similarly, any memory deallocation can
potentially result in hugepages being released back to the system.

Memory allocated in this mode is not guaranteed to be IOVA-contiguous. If large
chunks of IOVA-contiguous are required (with "large" defined as "more than one
page"), it is recommended to either use VFIO driver for all physical devices (so
that IOVA and VA addresses can be the same, thereby bypassing physical addresses
entirely), or use legacy memory mode.

For chunks of memory which must be IOVA-contiguous, it is recommended to use
``rte_memzone_reserve()`` function with ``RTE_MEMZONE_IOVA_CONTIG`` flag
specified. This way, memory allocator will ensure that, whatever memory mode is
in use, either reserved memory will satisfy the requirements, or the allocation
will fail.

There is no need to preallocate any memory at startup using ``-m`` or
``--socket-mem`` command-line parameters, however it is still possible to do so,
in which case preallocate memory will be "pinned" (i.e. will never be released
by the application back to the system). It will be possible to allocate more
hugepages, and deallocate those, but any preallocated pages will not be freed.
If neither ``-m`` nor ``--socket-mem`` were specified, no memory will be
preallocated, and all memory will be allocated at runtime, as needed.

Another available option to use in dynamic memory mode is
``--single-file-segments`` command-line option. This option will put pages in
single files (per memseg list), as opposed to creating a file per page. This is
normally not needed, but can be useful for use cases like userspace vhost, where
there is limited number of page file descriptors that can be passed to VirtIO.

If the application (or DPDK-internal code, such as device drivers) wishes to
receive notifications about newly allocated memory, it is possible to register
for memory event callbacks via ``rte_mem_event_callback_register()`` function.
This will call a callback function any time DPDK's memory map has changed.

If the application (or DPDK-internal code, such as device drivers) wishes to be
notified about memory allocations above specified threshold (and have a chance
to deny them), allocation validator callbacks are also available via
``rte_mem_alloc_validator_callback_register()`` function.

A default validator callback is provided by EAL, which can be enabled with a
``--socket-limit`` command-line option, for a simple way to limit maximum amount
of memory that can be used by DPDK application.

.. warning::
    Memory subsystem uses DPDK IPC internally, so memory allocations/callbacks
    and IPC must not be mixed: it is not safe to allocate/free memory inside
    memory-related or IPC callbacks, and it is not safe to use IPC inside
    memory-related callbacks. See chapter
    :ref:`Multi-process Support <Multi-process_Support>` for more details about
    DPDK IPC.

+ Legacy memory mode

This mode is enabled by specifying ``--legacy-mem`` command-line switch to the
EAL. This switch will have no effect on FreeBSD as FreeBSD only supports
legacy mode anyway.

This mode mimics historical behavior of EAL. That is, EAL will reserve all
memory at startup, sort all memory into large IOVA-contiguous chunks, and will
not allow acquiring or releasing hugepages from the system at runtime.

If neither ``-m`` nor ``--socket-mem`` were specified, the entire available
hugepage memory will be preallocated.

+ Hugepage allocation matching

This behavior is enabled by specifying the ``--match-allocations`` command-line
switch to the EAL. This switch is Linux-only and not supported with
``--legacy-mem`` nor ``--no-huge``.

Some applications using memory event callbacks may require that hugepages be
freed exactly as they were allocated. These applications may also require
that any allocation from the malloc heap not span across allocations
associated with two different memory event callbacks. Hugepage allocation
matching can be used by these types of applications to satisfy both of these
requirements. This can result in some increased memory usage which is
very dependent on the memory allocation patterns of the application.

+ 32-bit support

Additional restrictions are present when running in 32-bit mode. In dynamic
memory mode, by default maximum of 2 gigabytes of VA space will be preallocated,
and all of it will be on main lcore NUMA node unless ``--socket-mem`` flag is
used.

In legacy mode, VA space will only be preallocated for segments that were
requested (plus padding, to keep IOVA-contiguousness).

+ Maximum amount of memory

All possible virtual memory space that can ever be used for hugepage mapping in
a DPDK process is preallocated at startup, thereby placing an upper limit on how
much memory a DPDK application can have. DPDK memory is stored in segment lists,
each segment is strictly one physical page. It is possible to change the amount
of virtual memory being preallocated at startup by editing the following config
variables:

* ``RTE_MAX_MEMSEG_LISTS`` controls how many segment lists can DPDK have
* ``RTE_MAX_MEM_MB_PER_LIST`` controls how much megabytes of memory each
  segment list can address
* ``RTE_MAX_MEMSEG_PER_LIST`` controls how many segments each segment list
  can have
* ``RTE_MAX_MEMSEG_PER_TYPE`` controls how many segments each memory type
  can have (where "type" is defined as "page size + NUMA node" combination)
* ``RTE_MAX_MEM_MB_PER_TYPE`` controls how much megabytes of memory each
  memory type can address
* ``RTE_MAX_MEM_MB`` places a global maximum on the amount of memory
  DPDK can reserve

Normally, these options do not need to be changed.

.. note::

    Preallocated virtual memory is not to be confused with preallocated hugepage
    memory! All DPDK processes preallocate virtual memory at startup. Hugepages
    can later be mapped into that preallocated VA space (if dynamic memory mode
    is enabled), and can optionally be mapped into it at startup.

+ Segment file descriptors

On Linux, in most cases, EAL will store segment file descriptors in EAL. This
can become a problem when using smaller page sizes due to underlying limitations
of ``glibc`` library. For example, Linux API calls such as ``select()`` may not
work correctly because ``glibc`` does not support more than certain number of
file descriptors.

There are two possible solutions for this problem. The recommended solution is
to use ``--single-file-segments`` mode, as that mode will not use a file
descriptor per each page, and it will keep compatibility with Virtio with
vhost-user backend. This option is not available when using ``--legacy-mem``
mode.

Another option is to use bigger page sizes. Since fewer pages are required to
cover the same memory area, fewer file descriptors will be stored internally
by EAL.

Support for Externally Allocated Memory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is possible to use externally allocated memory in DPDK. There are two ways in
which using externally allocated memory can work: the malloc heap API's, and
manual memory management.

+ Using heap API's for externally allocated memory

Using a set of malloc heap API's is the recommended way to use externally
allocated memory in DPDK. In this way, support for externally allocated memory
is implemented through overloading the socket ID - externally allocated heaps
will have socket ID's that would be considered invalid under normal
circumstances. Requesting an allocation to take place from a specified
externally allocated memory is a matter of supplying the correct socket ID to
DPDK allocator, either directly (e.g. through a call to ``rte_malloc``) or
indirectly (through data structure-specific allocation API's such as
``rte_ring_create``). Using these API's also ensures that mapping of externally
allocated memory for DMA is also performed on any memory segment that is added
to a DPDK malloc heap.

Since there is no way DPDK can verify whether memory is available or valid, this
responsibility falls on the shoulders of the user. All multiprocess
synchronization is also user's responsibility, as well as ensuring  that all
calls to add/attach/detach/remove memory are done in the correct order. It is
not required to attach to a memory area in all processes - only attach to memory
areas as needed.

The expected workflow is as follows:

* Get a pointer to memory area
* Create a named heap
* Add memory area(s) to the heap
    - If IOVA table is not specified, IOVA addresses will be assumed to be
      unavailable, and DMA mappings will not be performed
    - Other processes must attach to the memory area before they can use it
* Get socket ID used for the heap
* Use normal DPDK allocation procedures, using supplied socket ID
* If memory area is no longer needed, it can be removed from the heap
    - Other processes must detach from this memory area before it can be removed
* If heap is no longer needed, remove it
    - Socket ID will become invalid and will not be reused

For more information, please refer to ``rte_malloc`` API documentation,
specifically the ``rte_malloc_heap_*`` family of function calls.

+ Using externally allocated memory without DPDK API's

While using heap API's is the recommended method of using externally allocated
memory in DPDK, there are certain use cases where the overhead of DPDK heap API
is undesirable - for example, when manual memory management is performed on an
externally allocated area. To support use cases where externally allocated
memory will not be used as part of normal DPDK workflow, there is also another
set of API's under the ``rte_extmem_*`` namespace.

These API's are (as their name implies) intended to allow registering or
unregistering externally allocated memory to/from DPDK's internal page table, to
allow API's like ``rte_mem_virt2memseg`` etc. to work with externally allocated
memory. Memory added this way will not be available for any regular DPDK
allocators; DPDK will leave this memory for the user application to manage.

The expected workflow is as follows:

* Get a pointer to memory area
* Register memory within DPDK
    - If IOVA table is not specified, IOVA addresses will be assumed to be
      unavailable
    - Other processes must attach to the memory area before they can use it
* Perform DMA mapping with ``rte_dev_dma_map`` if needed
* Use the memory area in your application
* If memory area is no longer needed, it can be unregistered
    - If the area was mapped for DMA, unmapping must be performed before
      unregistering memory
    - Other processes must detach from the memory area before it can be
      unregistered

Since these externally allocated memory areas will not be managed by DPDK, it is
therefore up to the user application to decide how to use them and what to do
with them once they're registered.

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

The EAL can query the CPU at runtime (using the rte_cpu_get_features() function) to determine which CPU features are available.

User Space Interrupt Event
~~~~~~~~~~~~~~~~~~~~~~~~~~

+ User Space Interrupt and Alarm Handling in Host Thread

The EAL creates a host thread to poll the UIO device file descriptors to detect the interrupts.
Callbacks can be registered or unregistered by the EAL functions for a specific interrupt event
and are called in the host thread asynchronously.
The EAL also allows timed callbacks to be used in the same way as for NIC interrupts.

.. note::

    In DPDK PMD, the only interrupts handled by the dedicated host thread are those for link status change
    (link up and link down notification) and for sudden device removal.


+ RX Interrupt Event

The receive and transmit routines provided by each PMD don't limit themselves to execute in polling thread mode.
To ease the idle polling with tiny throughput, it's useful to pause the polling and wait until the wake-up event happens.
The RX interrupt is the first choice to be such kind of wake-up event, but probably won't be the only one.

EAL provides the event APIs for this event-driven thread mode.
Taking Linux as an example, the implementation relies on epoll. Each thread can monitor an epoll instance
in which all the wake-up events' file descriptors are added. The event file descriptors are created and mapped to
the interrupt vectors according to the UIO/VFIO spec.
From FreeBSD's perspective, kqueue is the alternative way, but not implemented yet.

EAL initializes the mapping between event file descriptors and interrupt vectors, while each device initializes the mapping
between interrupt vectors and queues. In this way, EAL actually is unaware of the interrupt cause on the specific vector.
The eth_dev driver takes responsibility to program the latter mapping.

.. note::

    Per queue RX interrupt event is only allowed in VFIO which supports multiple MSI-X vector. In UIO, the RX interrupt
    together with other interrupt causes shares the same vector. In this case, when RX interrupt and LSC(link status change)
    interrupt are both enabled(intr_conf.lsc == 1 && intr_conf.rxq == 1), only the former is capable.

The RX interrupt are controlled/enabled/disabled by ethdev APIs - 'rte_eth_dev_rx_intr_*'. They return failure if the PMD
hasn't support them yet. The intr_conf.rxq flag is used to turn on the capability of RX interrupt per device.

+ Device Removal Event

This event is triggered by a device being removed at a bus level. Its
underlying resources may have been made unavailable (i.e. PCI mappings
unmapped). The PMD must make sure that on such occurrence, the application can
still safely use its callbacks.

This event can be subscribed to in the same way one would subscribe to a link
status change event. The execution context is thus the same, i.e. it is the
dedicated interrupt host thread.

Considering this, it is likely that an application would want to close a
device having emitted a Device Removal Event. In such case, calling
``rte_eth_dev_close()`` can trigger it to unregister its own Device Removal Event
callback. Care must be taken not to close the device from the interrupt handler
context. It is necessary to reschedule such closing operation.

Block list
~~~~~~~~~~

The EAL PCI device block list functionality can be used to mark certain NIC ports as unavailable,
so they are ignored by the DPDK.
The ports to be blocked are identified using the PCIe* description (Domain:Bus:Device.Function).

Misc Functions
~~~~~~~~~~~~~~

Locks and atomic operations are per-architecture (i686 and x86_64).

IOVA Mode Detection
~~~~~~~~~~~~~~~~~~~

IOVA Mode is selected by considering what the current usable Devices on the
system require and/or support.

On FreeBSD, RTE_IOVA_PA is always the default. On Linux, the IOVA mode is
detected based on a 2-step heuristic detailed below.

For the first step, EAL asks each bus its requirement in terms of IOVA mode
and decides on a preferred IOVA mode.

- if all buses report RTE_IOVA_PA, then the preferred IOVA mode is RTE_IOVA_PA,
- if all buses report RTE_IOVA_VA, then the preferred IOVA mode is RTE_IOVA_VA,
- if all buses report RTE_IOVA_DC, no bus expressed a preference, then the
  preferred mode is RTE_IOVA_DC,
- if the buses disagree (at least one wants RTE_IOVA_PA and at least one wants
  RTE_IOVA_VA), then the preferred IOVA mode is RTE_IOVA_DC (see below with the
  check on Physical Addresses availability),

If the buses have expressed no preference on which IOVA mode to pick, then a
default is selected using the following logic:

- if physical addresses are not available, RTE_IOVA_VA mode is used
- if /sys/kernel/iommu_groups is not empty, RTE_IOVA_VA mode is used
- otherwise, RTE_IOVA_PA mode is used

In the case when the buses had disagreed on their preferred IOVA mode, part of
the buses won't work because of this decision.

The second step checks if the preferred mode complies with the Physical
Addresses availability since those are only available to root user in recent
kernels. Namely, if the preferred mode is RTE_IOVA_PA but there is no access to
Physical Addresses, then EAL init fails early, since later probing of the
devices would fail anyway.

.. note::

    The RTE_IOVA_VA mode is preferred as the default in most cases for the
    following reasons:

    - All drivers are expected to work in RTE_IOVA_VA mode, irrespective of
      physical address availability.
    - By default, the mempool, first asks for IOVA-contiguous memory using
      ``RTE_MEMZONE_IOVA_CONTIG``. This is slow in RTE_IOVA_PA mode and it may
      affect the application boot time.
    - It is easy to enable large amount of IOVA-contiguous memory use cases
      with IOVA in VA mode.

    It is expected that all PCI drivers work in both RTE_IOVA_PA and
    RTE_IOVA_VA modes.

    If a PCI driver does not support RTE_IOVA_PA mode, the
    ``RTE_PCI_DRV_NEED_IOVA_AS_VA`` flag is used to dictate that this PCI
    driver can only work in RTE_IOVA_VA mode.

    When the KNI kernel module is detected, RTE_IOVA_PA mode is preferred as a
    performance penalty is expected in RTE_IOVA_VA mode.

IOVA Mode Configuration
~~~~~~~~~~~~~~~~~~~~~~~

Auto detection of the IOVA mode, based on probing the bus and IOMMU configuration, may not report
the desired addressing mode when virtual devices that are not directly attached to the bus are present.
To facilitate forcing the IOVA mode to a specific value the EAL command line option ``--iova-mode`` can
be used to select either physical addressing('pa') or virtual addressing('va').

.. _max_simd_bitwidth:


Max SIMD bitwidth
~~~~~~~~~~~~~~~~~

The EAL provides a single setting to limit the max SIMD bitwidth used by DPDK,
which is used in determining the vector path, if any, chosen by a component.
The value can be set at runtime by an application using the
'rte_vect_set_max_simd_bitwidth(uint16_t bitwidth)' function,
which should only be called once at initialization, before EAL init.
The value can be overridden by the user using the EAL command-line option '--force-max-simd-bitwidth'.

When choosing a vector path, along with checking the CPU feature support,
the value of the max SIMD bitwidth must also be checked, and can be retrieved using the
'rte_vect_get_max_simd_bitwidth()' function.
The value should be compared against the enum values for accepted max SIMD bitwidths:

.. code-block:: c

   enum rte_vect_max_simd {
       RTE_VECT_SIMD_DISABLED = 64,
       RTE_VECT_SIMD_128 = 128,
       RTE_VECT_SIMD_256 = 256,
       RTE_VECT_SIMD_512 = 512,
       RTE_VECT_SIMD_MAX = INT16_MAX + 1,
   };

    if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
        /* Take AVX-512 vector path */
    else if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256)
        /* Take AVX2 vector path */


Memory Segments and Memory Zones (memzone)
------------------------------------------

The mapping of physical memory is provided by this feature in the EAL.
As physical memory can have gaps, the memory is described in a table of descriptors,
and each descriptor (called rte_memseg ) describes a physical page.

On top of this, the memzone allocator's role is to reserve contiguous portions of physical memory.
These zones are identified by a unique name when the memory is reserved.

The rte_memzone descriptors are also located in the configuration structure.
This structure is accessed using rte_eal_get_configuration().
The lookup (by name) of a memory zone returns a descriptor containing the physical address of the memory zone.

Memory zones can be reserved with specific start address alignment by supplying the align parameter
(by default, they are aligned to cache line size).
The alignment value should be a power of two and not less than the cache line size (64 bytes).
Memory zones can also be reserved from either 2 MB or 1 GB hugepages, provided that both are available on the system.

Both memsegs and memzones are stored using ``rte_fbarray`` structures. Please
refer to *DPDK API Reference* for more information.


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

It is possible to use the DPDK execution context with any user pthread (aka. non-EAL pthreads).
There are two kinds of non-EAL pthreads:

- a registered non-EAL pthread with a valid *_lcore_id* that was successfully assigned by calling ``rte_thread_register()``,
- a non registered non-EAL pthread with a LCORE_ID_ANY,

For non registered non-EAL pthread (with a LCORE_ID_ANY *_lcore_id*), some libraries will use an alternative unique ID (e.g. TID), some will not be impacted at all, and some will work but with limitations (e.g. timer and mempool libraries).

All these impacts are mentioned in :ref:`known_issue_label` section.

Public Thread API
~~~~~~~~~~~~~~~~~

There are two public APIs ``rte_thread_set_affinity()`` and ``rte_thread_get_affinity()`` introduced for threads.
When they're used in any pthread context, the Thread Local Storage(TLS) will be set/get.

Those TLS include *_cpuset* and *_socket_id*:

*	*_cpuset* stores the CPUs bitmap to which the pthread is affinitized.

*	*_socket_id* stores the NUMA node of the CPU set. If the CPUs in CPU set belong to different NUMA node, the *_socket_id* will be set to SOCKET_ID_ANY.


Control Thread API
~~~~~~~~~~~~~~~~~~

It is possible to create Control Threads using the public API
``rte_ctrl_thread_create()``.
Those threads can be used for management/infrastructure tasks and are used
internally by DPDK for multi process support and interrupt handling.

Those threads will be scheduled on CPUs part of the original process CPU
affinity from which the dataplane and service lcores are excluded.

For example, on a 8 CPUs system, starting a dpdk application with -l 2,3
(dataplane cores), then depending on the affinity configuration which can be
controlled with tools like taskset (Linux) or cpuset (FreeBSD),

- with no affinity configuration, the Control Threads will end up on
  0-1,4-7 CPUs.
- with affinity restricted to 2-4, the Control Threads will end up on
  CPU 4.
- with affinity restricted to 2-3, the Control Threads will end up on
  CPU 2 (main lcore, which is the default when no CPU is available).

.. _known_issue_label:

Known Issues
~~~~~~~~~~~~

+ rte_mempool

  The rte_mempool uses a per-lcore cache inside the mempool.
  For unregistered non-EAL pthreads, ``rte_lcore_id()`` will not return a valid number.
  So for now, when rte_mempool is used with unregistered non-EAL pthreads, the put/get operations will bypass the default mempool cache and there is a performance penalty because of this bypass.
  Only user-owned external caches can be used in an unregistered non-EAL context in conjunction with ``rte_mempool_generic_put()`` and ``rte_mempool_generic_get()`` that accept an explicit cache parameter.

+ rte_ring

  rte_ring supports multi-producer enqueue and multi-consumer dequeue.
  However, it is non-preemptive, this has a knock on effect of making rte_mempool non-preemptible.

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

  This means, use cases involving preemptible pthreads should consider using rte_ring carefully.

  1. It CAN be used for preemptible single-producer and single-consumer use case.

  2. It CAN be used for non-preemptible multi-producer and preemptible single-consumer use case.

  3. It CAN be used for preemptible single-producer and non-preemptible multi-consumer use case.

  4. It MAY be used by preemptible multi-producer and/or preemptible multi-consumer pthreads whose scheduling policy are all SCHED_OTHER(cfs), SCHED_IDLE or SCHED_BATCH. User SHOULD be aware of the performance penalty before using it.

  5. It MUST not be used by multi-producer/consumer pthreads, whose scheduling policies are SCHED_FIFO or SCHED_RR.

  Alternatively, applications can use the lock-free stack mempool handler. When
  considering this handler, note that:

  - It is currently limited to the aarch64 and x86_64 platforms, because it uses
    an instruction (16-byte compare-and-swap) that is not yet available on other
    platforms.
  - It has worse average-case performance than the non-preemptive rte_ring, but
    software caching (e.g. the mempool cache) can mitigate this by reducing the
    number of stack accesses.

+ rte_timer

  Running  ``rte_timer_manage()`` on an unregistered non-EAL pthread is not allowed. However, resetting/stopping the timer from a non-EAL pthread is allowed.

+ rte_log

  In unregistered non-EAL pthreads, there is no per thread loglevel and logtype, global loglevels are used.

+ misc

  The debug statistics of rte_ring, rte_mempool and rte_timer are not supported in an unregistered non-EAL pthread.

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

*   first - this points to the first element in the heap.

*   last - this points to the last element in the heap.

.. _figure_malloc_heap:

.. figure:: img/malloc_heap.*

   Example of a malloc heap and malloc elements within the malloc library


.. _malloc_elem:

Structure: malloc_elem
""""""""""""""""""""""

The malloc_elem structure is used as a generic header structure for various
blocks of memory.
It is used in two different ways - all shown in the diagram above:

#.  As a header on a block of free or allocated memory - normal case

#.  As a padding header inside a block of memory

The most important fields in the structure and how they are used are described below.

Malloc heap is a doubly-linked list, where each element keeps track of its
previous and next elements. Due to the fact that hugepage memory can come and
go, neighboring malloc elements may not necessarily be adjacent in memory.
Also, since a malloc element may span multiple pages, its contents may not
necessarily be IOVA-contiguous either - each malloc element is only guaranteed
to be virtually contiguous.

.. note::

    If the usage of a particular field in one of the above three usages is not
    described, the field can be assumed to have an undefined value in that
    situation, for example, for padding headers only the "state" and "pad"
    fields have valid values.

*   heap - this pointer is a reference back to the heap structure from which
    this block was allocated.
    It is used for normal memory blocks when they are being freed, to add the
    newly-freed block to the heap's free-list.

*   prev - this pointer points to previous header element/block in memory. When
    freeing a block, this pointer is used to reference the previous block to
    check if that block is also free. If so, and the two blocks are immediately
    adjacent to each other, then the two free blocks are merged to form a single
    larger block.

*   next - this pointer points to next header element/block in memory. When
    freeing a block, this pointer is used to reference the next block to check
    if that block is also free. If so, and the two blocks are immediately
    adjacent to each other, then the two free blocks are merged to form a single
    larger block.

*   free_list - this is a structure pointing to previous and next elements in
    this heap's free list.
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

*   pad - this holds the length of the padding present at the start of the block.
    In the case of a normal block header, it is added to the address of the end
    of the header to give the address of the start of the data area, i.e. the
    value passed back to the application on a malloc.
    Within a dummy header inside the padding, this same value is stored, and is
    subtracted from the address of the dummy header to yield the address of the
    actual block header.

*   size - the size of the data block, including the header itself.

Memory Allocation
^^^^^^^^^^^^^^^^^

On EAL initialization, all preallocated memory segments are setup as part of the
malloc heap. This setup involves placing an :ref:`element header<malloc_elem>`
with ``FREE`` at the start of each virtually contiguous segment of memory.
The ``FREE`` element is then added to the ``free_list`` for the malloc heap.

This setup also happens whenever memory is allocated at runtime (if supported),
in which case newly allocated pages are also added to the heap, merging with any
adjacent free segments if there are any.

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
on the free list just has its size value adjusted, and the next/previous elements
have their "prev"/"next" pointers redirected to the newly created element.

In case when there is not enough memory in the heap to satisfy allocation
request, EAL will attempt to allocate more memory from the system (if supported)
and, following successful allocation, will retry reserving the memory again. In
a multiprocessing scenario, all primary and secondary processes will synchronize
their memory maps to ensure that any valid pointer to DPDK memory is guaranteed
to be valid at all times in all currently running processes.

Failure to synchronize memory maps in one of the processes will cause allocation
to fail, even though some of the processes may have allocated the memory
successfully. The memory is not added to the malloc heap unless primary process
has ensured that all other processes have mapped this memory successfully.

Any successful allocation event will trigger a callback, for which user
applications and other DPDK subsystems can register. Additionally, validation
callbacks will be triggered before allocation if the newly allocated memory will
exceed threshold set by the user, giving a chance to allow or deny allocation.

.. note::

    Any allocation of new pages has to go through primary process. If the
    primary process is not active, no memory will be allocated even if it was
    theoretically possible to do so. This is because primary's process map acts
    as an authority on what should or should not be mapped, while each secondary
    process has its own, local memory map. Secondary processes do not update the
    shared memory map, they only copy its contents to their local memory map.

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
and next elements. These next and previous elements are then checked to see if
they are also ``FREE`` and are immediately adjacent to the current one, and if
so, they are merged with the current element. This means that we can never have
two ``FREE`` memory blocks adjacent to one another, as they are always merged
into a single block.

If deallocating pages at runtime is supported, and the free element encloses
one or more pages, those pages can be deallocated and be removed from the heap.
If DPDK was started with command-line parameters for preallocating memory
(``-m`` or ``--socket-mem``), then those pages that were allocated at startup
will not be deallocated.

Any successful deallocation event will trigger a callback, for which user
applications and other DPDK subsystems can register.
