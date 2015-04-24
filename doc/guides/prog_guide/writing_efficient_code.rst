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

Writing Efficient Code
======================

This chapter provides some tips for developing efficient code using the DPDK.
For additional and more general information,
please refer to the *Intel® 64 and IA-32 Architectures Optimization Reference Manual*
which is a valuable reference to writing efficient code.

Memory
------

This section describes some key memory considerations when developing applications in the DPDK environment.

Memory Copy: Do not Use libc in the Data Plane
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Many libc functions are available in the DPDK, via the Linux* application environment.
This can ease the porting of applications and the development of the configuration plane.
However, many of these functions are not designed for performance.
Functions such as memcpy() or strcpy() should not be used in the data plane.
To copy small structures, the preference is for a simpler technique that can be optimized by the compiler.
Refer to the *VTune™ Performance Analyzer Essentials* publication from Intel Press for recommendations.

For specific functions that are called often,
it is also a good idea to provide a self-made optimized function, which should be declared as static inline.

The DPDK API provides an optimized rte_memcpy() function.

Memory Allocation
~~~~~~~~~~~~~~~~~

Other functions of libc, such as malloc(), provide a flexible way to allocate and free memory.
In some cases, using dynamic allocation is necessary,
but it is really not advised to use malloc-like functions in the data plane because
managing a fragmented heap can be costly and the allocator may not be optimized for parallel allocation.

If you really need dynamic allocation in the data plane, it is better to use a memory pool of fixed-size objects.
This API is provided by librte_mempool.
This data structure provides several services that increase performance, such as memory alignment of objects,
lockless access to objects, NUMA awareness, bulk get/put and per-lcore cache.
The rte_malloc () function uses a similar concept to mempools.

Concurrent Access to the Same Memory Area
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Read-Write (RW) access operations by several lcores to the same memory area can generate a lot of data cache misses,
which are very costly.
It is often possible to use per-lcore variables, for example, in the case of statistics.
There are at least two solutions for this:

*   Use RTE_PER_LCORE variables. Note that in this case, data on lcore X is not available to lcore Y.

*   Use a table of structures (one per lcore). In this case, each structure must be cache-aligned.

Read-mostly variables can be shared among lcores without performance losses if there are no RW variables in the same cache line.

NUMA
~~~~

On a NUMA system, it is preferable to access local memory since remote memory access is slower.
In the DPDK, the memzone, ring, rte_malloc and mempool APIs provide a way to create a pool on a specific socket.

Sometimes, it can be a good idea to duplicate data to optimize speed.
For read-mostly variables that are often accessed,
it should not be a problem to keep them in one socket only, since data will be present in cache.

Distribution Across Memory Channels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Modern memory controllers have several memory channels that can load or store data in parallel.
Depending on the memory controller and its configuration,
the number of channels and the way the memory is distributed across the channels varies.
Each channel has a bandwidth limit,
meaning that if all memory access operations are done on the first channel only, there is a potential bottleneck.

By default, the  :ref:`Mempool Library <Mempool_Library>` spreads the addresses of objects among memory channels.

Communication Between lcores
----------------------------

To provide a message-based communication between lcores,
it is advised to use the DPDK ring API, which provides a lockless ring implementation.

The ring supports bulk and burst access,
meaning that it is possible to read several elements from the ring with only one costly atomic operation
(see Chapter 5 "Ring Library").
Performance is greatly improved when using bulk access operations.

The code algorithm that dequeues messages may be something similar to the following:

.. code-block:: c

    #define MAX_BULK 32

    while (1) {
        /* Process as many elements as can be dequeued. */
        count = rte_ring_dequeue_burst(ring, obj_table, MAX_BULK);
        if (unlikely(count == 0))
            continue;

        my_process_bulk(obj_table, count);
   }

PMD Driver
----------

The DPDK Poll Mode Driver (PMD) is also able to work in bulk/burst mode,
allowing the factorization of some code for each call in the send or receive function.

Avoid partial writes.
When PCI devices write to system memory through DMA,
it costs less if the write operation is on a full cache line as opposed to part of it.
In the PMD code, actions have been taken to avoid partial writes as much as possible.

Lower Packet Latency
~~~~~~~~~~~~~~~~~~~~

Traditionally, there is a trade-off between throughput and latency.
An application can be tuned to achieve a high throughput,
but the end-to-end latency of an average packet will typically increase as a result.
Similarly, the application can be tuned to have, on average,
a low end-to-end latency, at the cost of lower throughput.

In order to achieve higher throughput,
the DPDK attempts to aggregate the cost of processing each packet individually by processing packets in bursts.

Using the testpmd application as an example,
the burst size can be set on the command line to a value of 16 (also the default value).
This allows the application to request 16 packets at a time from the PMD.
The testpmd application then immediately attempts to transmit all the packets that were received,
in this case, all 16 packets.

The packets are not transmitted until the tail pointer is updated on the corresponding TX queue of the network port.
This behavior is desirable when tuning for high throughput because
the cost of tail pointer updates to both the RX and TX queues can be spread across 16 packets,
effectively hiding the relatively slow MMIO cost of writing to the PCIe* device.
However, this is not very desirable when tuning for low latency because
the first packet that was received must also wait for another 15 packets to be received.
It cannot be transmitted until the other 15 packets have also been processed because
the NIC will not know to transmit the packets until the TX tail pointer has been updated,
which is not done until all 16 packets have been processed for transmission.

To consistently achieve low latency, even under heavy system load,
the application developer should avoid processing packets in bunches.
The testpmd application can be configured from the command line to use a burst value of 1.
This will allow a single packet to be processed at a time, providing lower latency,
but with the added cost of lower throughput.

Locks and Atomic Operations
---------------------------

Atomic operations imply a lock prefix before the instruction,
causing the processor's LOCK# signal to be asserted during execution of the following instruction.
This has a big impact on performance in a multicore environment.

Performance can be improved by avoiding lock mechanisms in the data plane.
It can often be replaced by other solutions like per-lcore variables.
Also, some locking techniques are more efficient than others.
For instance, the Read-Copy-Update (RCU) algorithm can frequently replace simple rwlocks.

Coding Considerations
---------------------

Inline Functions
~~~~~~~~~~~~~~~~

Small functions can be declared as static inline in the header file.
This avoids the cost of a call instruction (and the associated context saving).
However, this technique is not always efficient; it depends on many factors including the compiler.

Branch Prediction
~~~~~~~~~~~~~~~~~

The Intel® C/C++ Compiler (icc)/gcc built-in helper functions likely() and unlikely()
allow the developer to indicate if a code branch is likely to be taken or not.
For instance:

.. code-block:: c

    if (likely(x > 1))
        do_stuff();

Setting the Target CPU Type
---------------------------

The DPDK supports CPU microarchitecture-specific optimizations by means of CONFIG_RTE_MACHINE option
in the DPDK configuration file.
The degree of optimization depends on the compiler's ability to optimize for a specific microarchitecture,
therefore it is preferable to use the latest compiler versions whenever possible.

If the compiler version does not support the specific feature set (for example, the Intel® AVX instruction set),
the build process gracefully degrades to whatever latest feature set is supported by the compiler.

Since the build and runtime targets may not be the same,
the resulting binary also contains a platform check that runs before the
main() function and checks if the current machine is suitable for running the binary.

Along with compiler optimizations,
a set of preprocessor defines are automatically added to the build process (regardless of the compiler version).
These defines correspond to the instruction sets that the target CPU should be able to support.
For example, a binary compiled for any SSE4.2-capable processor will have RTE_MACHINE_CPUFLAG_SSE4_2 defined,
thus enabling compile-time code path selection for different platforms.
