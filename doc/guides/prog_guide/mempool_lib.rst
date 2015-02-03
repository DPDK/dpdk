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

.. _Mempool_Library:

Mempool Library
===============

A memory pool is an allocator of a fixed-sized object.
In the DPDK, it is identified by name and uses a ring to store free objects.
It provides some other optional services such as a per-core object cache and
an alignment helper to ensure that objects are padded to spread them equally on all DRAM or DDR3 channels.

This library is used by the
:ref:`Mbuf Library <Mbuf_Library>` and the
:ref:`Environment Abstraction Layer <Environment_Abstraction_Layer>` (for logging history).

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

Examples of alignment for different DIMM architectures are shown in Figure 5 and Figure 6.

.. _pg_figure_5:

**Figure 5. Two Channels and Quad-ranked DIMM Example**

.. image19_png has been replaced

|memory-management|

In this case, the assumption is that a packet is 16 blocks of 64 bytes, which is not true.

The Intel® 5520 chipset has three channels, so in most cases,
no padding is required between objects (except for objects whose size are n x 3 x 64 bytes blocks).

.. _pg_figure_6:

**Figure 6. Three Channels and Two Dual-ranked DIMM Example**

.. image20_png has been replaced

|memory-management2|

When creating a new pool, the user can specify to use this feature or not.

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
This cache can be enabled or disabled at creation of the pool.

The maximum size of the cache is static and is defined at compilation time (CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE).

Figure 7 shows a cache in operation.

.. _pg_figure_7:

**Figure 7. A mempool in Memory with its Associated Ring**

.. image21_png has been replaced

|mempool|

Use Cases
---------

All allocations that require a high level of performance should use a pool-based memory allocator.
Below are some examples:

*   :ref:`Mbuf Library <Mbuf_Library>`

*   :ref:`Environment Abstraction Layer <Environment_Abstraction_Layer>` , for logging service

*   Any application that needs to allocate fixed-sized objects in the data plane and that will be continuously utilized by the system.

.. |memory-management| image:: img/memory-management.*

.. |memory-management2| image:: img/memory-management2.*

.. |mempool| image:: img/mempool.*
