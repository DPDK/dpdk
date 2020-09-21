..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Arm Limited.

.. _RCU_Library:

RCU Library
============

Lockless data structures provide scalability and determinism.
They enable use cases where locking may not be allowed
(for example real-time applications).

In the following sections, the term "memory" refers to memory allocated
by typical APIs like malloc() or anything that is representative of
memory, for example an index of a free element array.

Since these data structures are lockless, the writers and readers
are accessing the data structures concurrently. Hence, while removing
an element from a data structure, the writers cannot return the memory
to the allocator, without knowing that the readers are not
referencing that element/memory anymore. Hence, it is required to
separate the operation of removing an element into two steps:

#. Delete: in this step, the writer removes the reference to the element from
   the data structure but does not return the associated memory to the
   allocator. This will ensure that new readers will not get a reference to
   the removed element. Removing the reference is an atomic operation.

#. Free (Reclaim): in this step, the writer returns the memory to the
   memory allocator only after knowing that all the readers have stopped
   referencing the deleted element.

This library helps the writer determine when it is safe to free the
memory by making use of thread Quiescent State (QS).

What is Quiescent State
-----------------------

Quiescent State can be defined as "any point in the thread execution where the
thread does not hold a reference to shared memory". It is the responsibility of
the application to determine its quiescent state.

Let us consider the following diagram:

.. _figure_quiescent_state:

.. figure:: img/rcu_general_info.*

   Phases in the Quiescent State model.


As shown in :numref:`figure_quiescent_state`, reader thread 1 accesses data
structures D1 and D2. When it is accessing D1, if the writer has to remove an
element from D1, the writer cannot free the memory associated with that
element immediately. The writer can return the memory to the allocator only
after the reader stops referencing D1. In other words, reader thread RT1 has
to enter a quiescent state.

Similarly, since reader thread 2 is also accessing D1, the writer has to
wait till thread 2 enters quiescent state as well.

However, the writer does not need to wait for reader thread 3 to enter
quiescent state. Reader thread 3 was not accessing D1 when the delete
operation happened. So, reader thread 3 will not have a reference to the
deleted entry.

It can be noted that, the critical sections for D2 is a quiescent state
for D1. i.e. for a given data structure Dx, any point in the thread execution
that does not reference Dx is a quiescent state.

Since memory is not freed immediately, there might be a need for
provisioning of additional memory, depending on the application requirements.

Factors affecting the RCU mechanism
-----------------------------------

It is important to make sure that this library keeps the overhead of
identifying the end of grace period and subsequent freeing of memory,
to a minimum. The following paras explain how grace period and critical
section affect this overhead.

The writer has to poll the readers to identify the end of grace period.
Polling introduces memory accesses and wastes CPU cycles. The memory
is not available for reuse during the grace period. Longer grace periods
exasperate these conditions.

The length of the critical section and the number of reader threads
is proportional to the duration of the grace period. Keeping the critical
sections smaller will keep the grace period smaller. However, keeping the
critical sections smaller requires additional CPU cycles (due to additional
reporting) in the readers.

Hence, we need the characteristics of a small grace period and large critical
section. This library addresses these characteristics by allowing the writer
to do other work without having to block until the readers report their
quiescent state.

RCU in DPDK
-----------

For DPDK applications, the beginning and end of a ``while(1)`` loop (where no
references to shared data structures are kept) act as perfect quiescent
states. This will combine all the shared data structure accesses into a
single, large critical section which helps keep the overhead on the
reader side to a minimum.

DPDK supports a pipeline model of packet processing and service cores.
In these use cases, a given data structure may not be used by all the
workers in the application. The writer has to wait only for the workers that
use the data structure to report their quiescent state. To provide the required
flexibility, this library has a concept of a QS variable. If required, the
application can create one QS variable per data structure to help it track the
end of grace period for each data structure. This helps keep the length of grace
period to a minimum.

How to use this library
-----------------------

The application must allocate memory and initialize a QS variable.

Applications can call ``rte_rcu_qsbr_get_memsize()`` to calculate the size
of memory to allocate. This API takes a maximum number of reader threads,
using this variable, as a parameter.

Further, the application can initialize a QS variable using the API
``rte_rcu_qsbr_init()``.

Each reader thread is assumed to have a unique thread ID. Currently, the
management of the thread ID (for example allocation/free) is left to the
application. The thread ID should be in the range of 0 to
maximum number of threads provided while creating the QS variable.
The application could also use ``lcore_id`` as the thread ID where applicable.

The ``rte_rcu_qsbr_thread_register()`` API will register a reader thread
to report its quiescent state. This can be called from a reader thread.
A control plane thread can also call this on behalf of a reader thread.
The reader thread must call ``rte_rcu_qsbr_thread_online()`` API to start
reporting its quiescent state.

Some of the use cases might require the reader threads to make blocking API
calls (for example while using eventdev APIs). The writer thread should not
wait for such reader threads to enter quiescent state.  The reader thread must
call ``rte_rcu_qsbr_thread_offline()`` API, before calling blocking APIs. It
can call ``rte_rcu_qsbr_thread_online()`` API once the blocking API call
returns.

The writer thread can trigger the reader threads to report their quiescent
state by calling the API ``rte_rcu_qsbr_start()``. It is possible for multiple
writer threads to query the quiescent state status simultaneously. Hence,
``rte_rcu_qsbr_start()`` returns a token to each caller.

The writer thread must call ``rte_rcu_qsbr_check()`` API with the token to
get the current quiescent state status. Option to block till all the reader
threads enter the quiescent state is provided. If this API indicates that
all the reader threads have entered the quiescent state, the application
can free the deleted entry.

The APIs ``rte_rcu_qsbr_start()`` and ``rte_rcu_qsbr_check()`` are lock free.
Hence, they can be called concurrently from multiple writers even while
running as worker threads.

The separation of triggering the reporting from querying the status provides
the writer threads flexibility to do useful work instead of blocking for the
reader threads to enter the quiescent state or go offline. This reduces the
memory accesses due to continuous polling for the status. But, since the
resource is freed at a later time, the token and the reference to the deleted
resource need to be stored for later queries.

The ``rte_rcu_qsbr_synchronize()`` API combines the functionality of
``rte_rcu_qsbr_start()`` and blocking ``rte_rcu_qsbr_check()`` into a single
API. This API triggers the reader threads to report their quiescent state and
polls till all the readers enter the quiescent state or go offline. This API
does not allow the writer to do useful work while waiting and introduces
additional memory accesses due to continuous polling. However, the application
does not have to store the token or the reference to the deleted resource. The
resource can be freed immediately after ``rte_rcu_qsbr_synchronize()`` API
returns.

The reader thread must call ``rte_rcu_qsbr_thread_offline()`` and
``rte_rcu_qsbr_thread_unregister()`` APIs to remove itself from reporting its
quiescent state. The ``rte_rcu_qsbr_check()`` API will not wait for this reader
thread to report the quiescent state status anymore.

The reader threads should call ``rte_rcu_qsbr_quiescent()`` API to indicate that
they entered a quiescent state. This API checks if a writer has triggered a
quiescent state query and update the state accordingly.

The ``rte_rcu_qsbr_lock()`` and ``rte_rcu_qsbr_unlock()`` are empty functions.
However, these APIs can aid in debugging issues. One can mark the access to
shared data structures on the reader side using these APIs. The
``rte_rcu_qsbr_quiescent()`` will check if all the locks are unlocked.

Resource reclamation framework for DPDK
---------------------------------------

Lock-free algorithms place additional burden of resource reclamation on
the application. When a writer deletes an entry from a data structure, the writer:

#. Has to start the grace period
#. Has to store a reference to the deleted resources in a FIFO
#. Should check if the readers have completed a grace period and free the resources.

There are several APIs provided to help with this process. The writer
can create a FIFO to store the references to deleted resources using ``rte_rcu_qsbr_dq_create()``.
The resources can be enqueued to this FIFO using ``rte_rcu_qsbr_dq_enqueue()``.
If the FIFO is full, ``rte_rcu_qsbr_dq_enqueue`` will reclaim the resources before enqueuing. It will also reclaim resources on regular basis to keep the FIFO from growing too large. If the writer runs out of resources, the writer can call ``rte_rcu_qsbr_dq_reclaim`` API to reclaim resources. ``rte_rcu_qsbr_dq_delete`` is provided to reclaim any remaining resources and free the FIFO while shutting down.

However, if this resource reclamation process were to be integrated in lock-free data structure libraries, it
hides this complexity from the application and makes it easier for the application to adopt lock-free algorithms. The following paragraphs discuss how the reclamation process can be integrated in DPDK libraries.

In any DPDK application, the resource reclamation process using QSBR can be split into 4 parts:

#. Initialization
#. Quiescent State Reporting
#. Reclaiming Resources
#. Shutdown

The design proposed here assigns different parts of this process to client libraries and applications. The term 'client library' refers to lock-free data structure libraries such at rte_hash, rte_lpm etc. in DPDK or similar libraries outside of DPDK. The term 'application' refers to the packet processing application that makes use of DPDK such as L3 Forwarding example application, OVS, VPP etc..

The application has to handle 'Initialization' and 'Quiescent State Reporting'. So,

* the application has to create the RCU variable and register the reader threads to report their quiescent state.
* the application has to register the same RCU variable with the client library.
* reader threads in the application have to report the quiescent state. This allows for the application to control the length of the critical section/how frequently the application wants to report the quiescent state.

The client library will handle 'Reclaiming Resources' part of the process. The
client libraries will make use of the writer thread context to execute the memory
reclamation algorithm. So,

* client library should provide an API to register a RCU variable that it will use. It should call ``rte_rcu_qsbr_dq_create()`` to create the FIFO to store the references to deleted entries.
* client library should use ``rte_rcu_qsbr_dq_enqueue`` to enqueue the deleted resources on the FIFO and start the grace period.
* if the library runs out of resources while adding entries, it should call ``rte_rcu_qsbr_dq_reclaim`` to reclaim the resources and try the resource allocation again.

The 'Shutdown' process needs to be shared between the application and the
client library.

* the application should make sure that the reader threads are not using the shared data structure, unregister the reader threads from the QSBR variable before calling the client library's shutdown function.

* client library should call ``rte_rcu_qsbr_dq_delete`` to reclaim any remaining resources and free the FIFO.

Integrating the resource reclamation with client libraries removes the burden from
the application and makes it easy to use lock-free algorithms.

This design has several advantages over currently known methods.

#. Application does not need a dedicated thread to reclaim resources. Memory
   reclamation happens as part of the writer thread with little impact on
   performance.
#. The client library has better control over the resources. For example: the client
   library can attempt to reclaim when it has run out of resources.
