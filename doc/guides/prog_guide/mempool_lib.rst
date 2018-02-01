..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Mempool_Library:

Mempool Library
===============

A memory pool is an allocator of a fixed-sized object.
In the DPDK, it is identified by name and uses a mempool handler to store free objects.
The default mempool handler is ring based.
It provides some other optional services such as a per-core object cache and
an alignment helper to ensure that objects are padded to spread them equally on all DRAM or DDR3 channels.

This library is used by the :ref:`Mbuf Library <Mbuf_Library>`.

Cookies
-------

In debug mode (CONFIG_RTE_LIBRTE_MEMPOOL_DEBUG is enabled), cookies are added at the beginning and end of allocated blocks.
The allocated objects then contain overwrite protection fields to help debugging buffer overflows.

Stats
-----

In debug mode (CONFIG_RTE_LIBRTE_MEMPOOL_DEBUG is enabled),
statistics about get from/put in the pool are stored in the mempool structure.
Statistics are per-lcore to avoid concurrent access to statistics counters.

Memory Alignment Constraints
----------------------------

Depending on hardware memory configuration, performance can be greatly improved by adding a specific padding between objects.
The objective is to ensure that the beginning of each object starts on a different channel and rank in memory so that all channels are equally loaded.

This is particularly true for packet buffers when doing L3 forwarding or flow classification.
Only the first 64 bytes are accessed, so performance can be increased by spreading the start addresses of objects among the different channels.

The number of ranks on any DIMM is the number of independent sets of DRAMs that can be accessed for the full data bit-width of the DIMM.
The ranks cannot be accessed simultaneously since they share the same data path.
The physical layout of the DRAM chips on the DIMM itself does not necessarily relate to the number of ranks.

When running an application, the EAL command line options provide the ability to add the number of memory channels and ranks.

.. note::

    The command line must always have the number of memory channels specified for the processor.

Examples of alignment for different DIMM architectures are shown in
:numref:`figure_memory-management` and :numref:`figure_memory-management2`.

.. _figure_memory-management:

.. figure:: img/memory-management.*

   Two Channels and Quad-ranked DIMM Example


In this case, the assumption is that a packet is 16 blocks of 64 bytes, which is not true.

The Intel® 5520 chipset has three channels, so in most cases,
no padding is required between objects (except for objects whose size are n x 3 x 64 bytes blocks).

.. _figure_memory-management2:

.. figure:: img/memory-management2.*

   Three Channels and Two Dual-ranked DIMM Example


When creating a new pool, the user can specify to use this feature or not.

.. _mempool_local_cache:

Local Cache
-----------

In terms of CPU usage, the cost of multiple cores accessing a memory pool's ring of free buffers may be high
since each access requires a compare-and-set (CAS) operation.
To avoid having too many access requests to the memory pool's ring,
the memory pool allocator can maintain a per-core cache and do bulk requests to the memory pool's ring,
via the cache with many fewer locks on the actual memory pool structure.
In this way, each core has full access to its own cache (with locks) of free objects and
only when the cache fills does the core need to shuffle some of the free objects back to the pools ring or
obtain more objects when the cache is empty.

While this may mean a number of buffers may sit idle on some core's cache,
the speed at which a core can access its own cache for a specific memory pool without locks provides performance gains.

The cache is composed of a small, per-core table of pointers and its length (used as a stack).
This internal cache can be enabled or disabled at creation of the pool.

The maximum size of the cache is static and is defined at compilation time (CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE).

:numref:`figure_mempool` shows a cache in operation.

.. _figure_mempool:

.. figure:: img/mempool.*

   A mempool in Memory with its Associated Ring

Alternatively to the internal default per-lcore local cache, an application can create and manage external caches through the ``rte_mempool_cache_create()``, ``rte_mempool_cache_free()`` and ``rte_mempool_cache_flush()`` calls.
These user-owned caches can be explicitly passed to ``rte_mempool_generic_put()`` and ``rte_mempool_generic_get()``.
The ``rte_mempool_default_cache()`` call returns the default internal cache if any.
In contrast to the default caches, user-owned caches can be used by non-EAL threads too.

Mempool Handlers
------------------------

This allows external memory subsystems, such as external hardware memory
management systems and software based memory allocators, to be used with DPDK.

There are two aspects to a mempool handler.

* Adding the code for your new mempool operations (ops). This is achieved by
  adding a new mempool ops code, and using the ``MEMPOOL_REGISTER_OPS`` macro.

* Using the new API to call ``rte_mempool_create_empty()`` and
  ``rte_mempool_set_ops_byname()`` to create a new mempool and specifying which
  ops to use.

Several different mempool handlers may be used in the same application. A new
mempool can be created by using the ``rte_mempool_create_empty()`` function,
then using ``rte_mempool_set_ops_byname()`` to point the mempool to the
relevant mempool handler callback (ops) structure.

Legacy applications may continue to use the old ``rte_mempool_create()`` API
call, which uses a ring based mempool handler by default. These applications
will need to be modified to use a new mempool handler.

For applications that use ``rte_pktmbuf_create()``, there is a config setting
(``RTE_MBUF_DEFAULT_MEMPOOL_OPS``) that allows the application to make use of
an alternative mempool handler.


Use Cases
---------

All allocations that require a high level of performance should use a pool-based memory allocator.
Below are some examples:

*   :ref:`Mbuf Library <Mbuf_Library>`

*   :ref:`Environment Abstraction Layer <Environment_Abstraction_Layer>` , for logging service

*   Any application that needs to allocate fixed-sized objects in the data plane and that will be continuously utilized by the system.
