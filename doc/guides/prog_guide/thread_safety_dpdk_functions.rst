..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Thread Safety of DPDK Functions
===============================

The DPDK is comprised of several libraries.
Some of the functions in these libraries can be safely called from multiple threads simultaneously, while others cannot.
This section allows the developer to take these issues into account when building their own application.

The run-time environment of the DPDK is typically a single thread per logical core.
In some cases, it is not only multi-threaded, but multi-process.
Typically, it is best to avoid sharing data structures between threads and/or processes where possible.
Where this is not possible, then the execution blocks must access the data in a thread- safe manner.
Mechanisms such as atomics or locking can be used that will allow execution blocks to operate serially.
However, this can have an effect on the performance of the application.

Fast-Path APIs
--------------

Applications operating in the data plane are performance sensitive but
certain functions within those libraries may not be safe to call from multiple threads simultaneously.
The hash, LPM and mempool libraries and RX/TX in the PMD are examples of this.

The hash and LPM libraries are, by design, thread unsafe in order to maintain performance.
However, if required the developer can add layers on top of these libraries to provide thread safety.
Locking is not needed in all situations, and in both the hash and LPM libraries,
lookups of values can be performed in parallel in multiple threads.
Adding, removing or modifying values, however,
cannot be done in multiple threads without using locking when a single hash or LPM table is accessed.
Another alternative to locking would be to create multiple instances of these tables allowing each thread its own copy.

The RX and TX of the PMD are the most critical aspects of a DPDK application
and it is recommended that no locking be used as it will impact performance.
Note, however, that these functions can safely be used from multiple threads
when each thread is performing I/O on a different NIC queue.
If multiple threads are to use the same hardware queue on the same NIC port,
then locking, or some other form of mutual exclusion, is necessary.

The ring library is based on a lockless ring-buffer algorithm that maintains its original design for thread safety.
Moreover, it provides high performance for either multi- or single-consumer/producer enqueue/dequeue operations.
The mempool library is based on the DPDK lockless ring library and therefore is also multi-thread safe.

Performance Insensitive API
---------------------------

Outside of the performance sensitive areas described in Section 25.1,
the DPDK provides a thread-safe API for most other libraries.
For example, malloc and memzone functions are safe for use in multi-threaded and multi-process environments.

The setup and configuration of the PMD is not performance sensitive, but is not thread safe either.
It is possible that the multiple read/writes during PMD setup and configuration could be corrupted in a multi-thread environment.
Since this is not performance sensitive, the developer can choose to add their own layer to provide thread-safe setup and configuration.
It is expected that, in most applications, the initial configuration of the network ports would be done by a single thread at startup.

Library Initialization
----------------------

It is recommended that DPDK libraries are initialized in the main thread at application startup
rather than subsequently in the forwarding threads.
However, the DPDK performs checks to ensure that libraries are only initialized once.
If initialization is attempted more than once, an error is returned.

In the multi-process case, the configuration information of shared memory will only be initialized by the primary process.
Thereafter, both primary and secondary processes can allocate/release any objects of memory that finally rely on rte_malloc or memzones.

Interrupt Thread
----------------

The DPDK works almost entirely in Linux user space in polling mode.
For certain infrequent operations, such as receiving a PMD link status change notification,
callbacks may be called in an additional thread outside the main DPDK processing threads.
These function callbacks should avoid manipulating DPDK objects that are also managed by the normal DPDK threads,
and if they need to do so,
it is up to the application to provide the appropriate locking or mutual exclusion restrictions around those objects.
