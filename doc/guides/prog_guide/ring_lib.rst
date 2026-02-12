..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Ring Library
============

The ring allows the management of queues.
Instead of having a linked list of infinite size, the rte_ring has the following properties:

*   FIFO

*   Maximum size is fixed, the objects are stored in a table

*   Objects can be pointers or elements of multiple of 4 byte size

*   Lockless implementation

*   Multi-consumer or single-consumer dequeue

*   Multi-producer or single-producer enqueue

*   Bulk dequeue - Dequeues the specified count of objects if successful; otherwise fails

*   Bulk enqueue - Enqueues the specified count of objects if successful; otherwise fails

*   Burst dequeue - Dequeue the maximum available objects if the specified count cannot be fulfilled

*   Burst enqueue - Enqueue the maximum available objects if the specified count cannot be fulfilled

The advantages of this data structure over a linked list queue are as follows:

*   Faster; only requires a single 32 bit Compare-And-Swap instruction instead of several pointer size Compare-And-Swap instructions.

*   Simpler than a full lockless queue.

*   Adapted to bulk enqueue/dequeue operations.
    As objects are stored in a table, a dequeue of several objects will not produce as many cache misses as in a linked queue.
    Also, a bulk dequeue of many objects does not cost more than a dequeue of a simple object.

The disadvantages:

*   Size is fixed

*   Having many rings costs more in terms of memory than a linked list queue. An empty ring contains at least N objects.

A simplified representation of a Ring is shown in with consumer and producer head and tail pointers to objects stored in the data structure.

.. _figure_ring1:

.. figure:: img/ring1.*

   Ring Structure


References for Ring Implementation in FreeBSD*
----------------------------------------------

The following code was added in FreeBSD 8.0, and is used in some network device drivers (at least in Intel drivers):

    * `bufring.h in FreeBSD <http://svn.freebsd.org/viewvc/base/release/8.0.0/sys/sys/buf_ring.h?revision=199625&amp;view=markup>`_

    * `bufring.c in FreeBSD <http://svn.freebsd.org/viewvc/base/release/8.0.0/sys/kern/subr_bufring.c?revision=199625&amp;view=markup>`_

Lockless Ring Buffer in Linux*
------------------------------

The following is a link describing the `Linux Lockless Ring Buffer Design <http://lwn.net/Articles/340400/>`_.

Additional Features
-------------------

Name
~~~~

A ring is identified by a unique name.
It is not possible to create two rings with the same name (rte_ring_create() returns NULL if this is attempted).

Use Cases
---------

Use cases for the Ring library include:

    *  Communication between applications in the DPDK

    *  Used by memory pool allocator

Anatomy of a Ring Buffer
------------------------

This section explains how a ring buffer operates.
The ring structure is composed of two head and tail couples; one is used by producers and one is used by the consumers.
The figures of the following sections refer to them as prod_head, prod_tail, cons_head and cons_tail.

Each figure represents a simplified state of the ring, which is a circular buffer.
The content of the function local variables is represented on the top of the figure,
and the content of ring structure is represented on the bottom of the figure.

Single Producer Enqueue
~~~~~~~~~~~~~~~~~~~~~~~

This section explains what occurs when a producer adds an object to the ring.
In this example, only the producer head and tail (prod_head and prod_tail) are modified,
and there is only one producer.

The initial state is to have a prod_head and prod_tail pointing at the same location.

Enqueue First Step
^^^^^^^^^^^^^^^^^^

First, *ring->prod_head* and ring->cons_tail are copied in local variables.
The prod_next local variable points to the next element of the table, or several elements after in case of bulk enqueue.

If there is not enough room in the ring (this is detected by checking cons_tail), it returns an error.


.. _figure_ring-enqueue1:

.. figure:: img/ring-enqueue1.*

   Enqueue first step


Enqueue Second Step
^^^^^^^^^^^^^^^^^^^

The second step is to modify *ring->prod_head* in ring structure to point to the same location as prod_next.

The added object is copied in the ring (obj4).


.. _figure_ring-enqueue2:

.. figure:: img/ring-enqueue2.*

   Enqueue second step


Enqueue Last Step
^^^^^^^^^^^^^^^^^

Once the object is added in the ring, ring->prod_tail in the ring structure is modified to point to the same location as *ring->prod_head*.
The enqueue operation is finished.


.. _figure_ring-enqueue3:

.. figure:: img/ring-enqueue3.*

   Enqueue last step


Single Consumer Dequeue
~~~~~~~~~~~~~~~~~~~~~~~

This section explains what occurs when a consumer dequeues an object from the ring.
In this example, only the consumer head and tail (cons_head and cons_tail) are modified and there is only one consumer.

The initial state is to have a cons_head and cons_tail pointing at the same location.

Dequeue First Step
^^^^^^^^^^^^^^^^^^

First, ring->cons_head and ring->prod_tail are copied in local variables.
The cons_next local variable points to the next element of the table, or several elements after in the case of bulk dequeue.

If there are not enough objects in the ring (this is detected by checking prod_tail), it returns an error.


.. _figure_ring-dequeue1:

.. figure:: img/ring-dequeue1.*

   Dequeue first step


Dequeue Second Step
^^^^^^^^^^^^^^^^^^^

The second step is to modify ring->cons_head in the ring structure to point to the same location as cons_next.

The dequeued object (obj1) is copied in the pointer given by the user.


.. _figure_ring-dequeue2:

.. figure:: img/ring-dequeue2.*

   Dequeue second step


Dequeue Last Step
^^^^^^^^^^^^^^^^^

Finally, ring->cons_tail in the ring structure is modified to point to the same location as ring->cons_head.
The dequeue operation is finished.


.. _figure_ring-dequeue3:

.. figure:: img/ring-dequeue3.*

   Dequeue last step


Multiple Producers Enqueue
~~~~~~~~~~~~~~~~~~~~~~~~~~

This section explains what occurs when two producers concurrently add an object to the ring.
In this example, only the producer head and tail (prod_head and prod_tail) are modified.

The initial state is to have a prod_head and prod_tail pointing at the same location.

Multiple Producers Enqueue First Step
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

On both cores, *ring->prod_head* and ring->cons_tail are copied in local variables.
The prod_next local variable points to the next element of the table,
or several elements after in the case of bulk enqueue.

If there is not enough room in the ring (this is detected by checking cons_tail), it returns an error.


.. _figure_ring-mp-enqueue1:

.. figure:: img/ring-mp-enqueue1.*

   Multiple producer enqueue first step


Multiple Producers Enqueue Second Step
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The second step is to modify ring->prod_head in the ring structure to point to the same location as prod_next.
This operation is done using a Compare And Swap (CAS) instruction, which does the following operations atomically:

*   If ring->prod_head is different to local variable prod_head,
    the CAS operation fails, and the code restarts at first step.

*   Otherwise, ring->prod_head is set to local prod_next,
    the CAS operation is successful, and processing continues.

In the figure, the operation succeeded on core 1, and step one restarted on core 2.


.. _figure_ring-mp-enqueue2:

.. figure:: img/ring-mp-enqueue2.*

   Multiple producer enqueue second step


Multiple Producers Enqueue Third Step
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The CAS operation is retried on core 2 with success.

The core 1 updates one element of the ring(obj4), and the core 2 updates another one (obj5).


.. _figure_ring-mp-enqueue3:

.. figure:: img/ring-mp-enqueue3.*

   Multiple producer enqueue third step


Multiple Producers Enqueue Fourth Step
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each core now wants to update ring->prod_tail.
A core can only update it if ring->prod_tail is equal to the prod_head local variable.
This is only true on core 1. The operation is finished on core 1.


.. _figure_ring-mp-enqueue4:

.. figure:: img/ring-mp-enqueue4.*

   Multiple producer enqueue fourth step


Multiple Producers Enqueue Last Step
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once ring->prod_tail is updated by core 1, core 2 is allowed to update it too.
The operation is also finished on core 2.


.. _figure_ring-mp-enqueue5:

.. figure:: img/ring-mp-enqueue5.*

   Multiple producer enqueue last step


Modulo 32-bit Indexes
~~~~~~~~~~~~~~~~~~~~~

In the preceding figures, the prod_head, prod_tail, cons_head and cons_tail indexes are represented by arrows.
In the actual implementation, these values are not between 0 and size(ring)-1 as would be assumed.
The indexes are between 0 and 2^32 -1, and we mask their value when we access the object table (the ring itself).
32-bit modulo also implies that operations on indexes (such as, add/subtract) will automatically do 2^32 modulo
if the result overflows the 32-bit number range.

The following are two examples that help to explain how indexes are used in a ring.

.. note::

    To simplify the explanation, operations with modulo 16-bit are used instead of modulo 32-bit.
    In addition, the four indexes are defined as unsigned 16-bit integers,
    as opposed to unsigned 32-bit integers in the more realistic case.


.. _figure_ring-modulo1:

.. figure:: img/ring-modulo1.*

   Modulo 32-bit indexes - Example 1


This ring contains 11000 entries.


.. _figure_ring-modulo2:

.. figure:: img/ring-modulo2.*

      Modulo 32-bit indexes - Example 2


This ring contains 12536 entries.

.. note::

    For ease of understanding, we use modulo 65536 operations in the above examples.
    In real execution cases, this is redundant for low efficiency, but is done automatically when the result overflows.

The code always maintains a distance between producer and consumer between 0 and size(ring)-1.
Thanks to this property, we can do subtractions between 2 index values in a modulo-32bit base:
that's why the overflow of the indexes is not a problem.

At any time, entries and free_entries are between 0 and size(ring)-1,
even if only the first term of subtraction has overflowed:

.. code-block:: c

    uint32_t entries = (prod_tail - cons_head);
    uint32_t free_entries = (mask + cons_tail -prod_head);

Producer/consumer synchronization modes
---------------------------------------

rte_ring supports different synchronization modes for producers and consumers.
These modes can be specified at ring creation/init time via ``flags``
parameter.
That should help users to configure ring in the most suitable way for his
specific usage scenarios.
Currently supported modes:

.. _Ring_Library_MPMC_Mode:

MP/MC (default one)
~~~~~~~~~~~~~~~~~~~

Multi-producer (/multi-consumer) mode. This is a default enqueue (/dequeue)
mode for the ring. In this mode multiple threads can enqueue (/dequeue)
objects to (/from) the ring. For 'classic' DPDK deployments (with one thread
per core) this is usually the most suitable and fastest synchronization mode.
As a well known limitation - it can perform quite pure on some overcommitted
scenarios.

.. _Ring_Library_SPSC_Mode:

SP/SC
~~~~~
Single-producer (/single-consumer) mode. In this mode only one thread at a time
is allowed to enqueue (/dequeue) objects to (/from) the ring.

.. _Ring_Library_MT_RTS_Mode:

MP_RTS/MC_RTS
~~~~~~~~~~~~~

Multi-producer (/multi-consumer) with Relaxed Tail Sync (RTS) mode.
The main difference from the original MP/MC algorithm is that
tail value is increased not by every thread that finished enqueue/dequeue,
but only by the last one.
That allows threads to avoid spinning on ring tail value,
leaving actual tail value change to the last thread at a given instance.
That technique helps to avoid the Lock-Waiter-Preemption (LWP) problem on tail
update and improves average enqueue/dequeue times on overcommitted systems.
To achieve that RTS requires 2 64-bit CAS for each enqueue(/dequeue) operation:
one for head update, second for tail update.
In comparison the original MP/MC algorithm requires one 32-bit CAS
for head update and waiting/spinning on tail value.

.. _Ring_Library_MT_HTS_Mode:

MP_HTS/MC_HTS
~~~~~~~~~~~~~

Multi-producer (/multi-consumer) with Head/Tail Sync (HTS) mode.
In that mode enqueue/dequeue operation is fully serialized:
at any given moment only one enqueue/dequeue operation can proceed.
This is achieved by allowing a thread to proceed with changing ``head.value``
only when ``head.value == tail.value``.
Both head and tail values are updated atomically (as one 64-bit value).
To achieve that 64-bit CAS is used by head update routine.
That technique also avoids the Lock-Waiter-Preemption (LWP) problem on tail
update and helps to improve ring enqueue/dequeue behavior in overcommitted
scenarios. Another advantage of fully serialized producer/consumer -
it provides the ability to implement MT safe peek API for rte_ring.

Ring Peek API
-------------

For ring with serialized producer/consumer (HTS sync mode) it is possible
to split public enqueue/dequeue API into two phases:

*   enqueue/dequeue start

*   enqueue/dequeue finish

That allows user to inspect objects in the ring without removing them
from it (aka MT safe peek) and reserve space for the objects in the ring
before actual enqueue.
Note that this API is available only for two sync modes:

*   Single Producer/Single Consumer (SP/SC)

*   Multi-producer/Multi-consumer with Head/Tail Sync (HTS)

It is a user responsibility to create/init ring with appropriate sync modes
selected. As an example of usage:

.. code-block:: c

    /* read 1 elem from the ring: */
    uint32_t n = rte_ring_dequeue_bulk_start(ring, &obj, 1, NULL);
    if (n != 0) {
        /* examine object */
        if (object_examine(obj) == KEEP)
            /* decided to keep it in the ring. */
            rte_ring_dequeue_finish(ring, 0);
        else
            /* decided to remove it from the ring. */
            rte_ring_dequeue_finish(ring, n);
    }

Note that between ``_start_`` and ``_finish_`` none other thread can proceed
with enqueue(/dequeue) operation till ``_finish_`` completes.

Ring Peek Zero Copy API
-----------------------

Along with the advantages of the peek APIs, zero copy APIs provide the ability
to copy the data to the ring memory directly without the need for temporary
storage (for ex: array of mbufs on the stack).

These APIs make it possible to split public enqueue/dequeue API into 3 phases:

* enqueue/dequeue start

* copy data to/from the ring

* enqueue/dequeue finish

Note that this API is available only for two sync modes:

*   Single Producer/Single Consumer (SP/SC)

*   Multi-producer/Multi-consumer with Head/Tail Sync (HTS)

It is a user responsibility to create/init ring with appropriate sync modes.
Following is an example of usage:

.. code-block:: c

    /* Reserve space on the ring */
    n = rte_ring_enqueue_zc_burst_start(r, 32, &zcd, NULL);
    /* Pkt I/O core polls packets from the NIC */
    if (n != 0) {
        nb_rx = rte_eth_rx_burst(portid, queueid, zcd->ptr1, zcd->n1);
        if (nb_rx == zcd->n1 && n != zcd->n1)
            nb_rx += rte_eth_rx_burst(portid, queueid, zcd->ptr2,
							n - zcd->n1);
        /* Provide packets to the packet processing cores */
        rte_ring_enqueue_zc_finish(r, nb_rx);
    }

Note that between ``_start_`` and ``_finish_`` no other thread can proceed
with enqueue(/dequeue) operation till ``_finish_`` completes.


Staged Ordered Ring API
-----------------------

Staged-Ordered-Ring (SORING) API provides a SW abstraction for *ordered* queues
with multiple processing *stages*.
It is based on conventional DPDK ``rte_ring`` API,
re-uses many of its concepts, and even substantial part of its code.
It can be viewed as an "extension" of ``rte_ring`` functionality.

In particular, main SORING properties:

* circular ring buffer with fixed size objects and related metadata.
* producer, consumer plus multiple processing stages in between.
* allows to split objects processing into multiple stages.
* objects remain in the same ring while moving from one stage to the other,
  initial order is preserved, no extra copying needed.
* preserves the ingress order of objects within the queue across multiple stages.
* each stage (and producer/consumer) can be served by single and/or multiple threads.
* number of stages, size and number of objects and their metadata in the ring
  are configurable at ring initialization time.

Data-Path API
~~~~~~~~~~~~~

SORING data-path API provided four main operations:

* ``enqueue``/``dequeue`` works in the same manner as for conventional ``rte_ring``,
  all ``rte_ring`` synchronization types are supported.

* ``acquire``/``release`` - for each stage there is an ``acquire`` (start)
  and ``release`` (finish) operation.
  After some objects are ``acquired`` - given thread can safely assume that
  it has exclusive possession of these objects till ``release`` for them is invoked.
  Note that right now user has to release exactly the same number of objects
  that was acquired before.
  After objects are ``released``, given thread loses its possession on them,
  and they can be either acquired by next stage or dequeued
  by the consumer (in case of last stage).

A simplified representation of a SORING with two stages is shown below.
On that picture ``obj5`` and ``obj4`` elements are acquired by stage 0,
``obj2`` and ``obj3`` are acquired by stage 1,
while ``obj1`` was already released by stage 1 and is ready to be consumed.

.. _figure_soring1:

.. figure:: img/soring-pic1.*

Along with traditional flavor there are enhanced versions for
all these data-path operations: ``enqueux``/``dequeux``/``acquirx``/``releasx``.
All enhanced versions take as extra parameter a pointer to an array of metadata values.
At initialization user can request within the ``soring`` supplementary
and optional array of metadata associated with each object in the ``soring``.
While ``soring`` element size is configurable and user can specify it big enough
to hold both object and its metadata together,
for performance reasons it might be plausible to access them as separate arrays.
Note that users are free to mix and match both versions of data-path API
in a way they like.
As an example, possible usage scenario when such separation helps:

.. code-block:: c

   /*
    * use pointer to mbuf as soring element, while tx_state as a metadata.
    * In this example we use a soring with just one stage.
    */
    union tx_state {
        /* negative values for error */
        int32_t rc;
        /* otherwise contain valid Tx port and queue IDs*/
        struct {
            uint16_t port_id;
            uint16_t queue_id;
        } tx;
    };
    struct rte_soring *soring;

producer/consumer part:

.. code-block:: c

   struct rte_mbuf *pkts[MAX_PKT_BURST];
   union tx_state txst[MAX_PKT_BURST];
   ...
   /* enqueue - writes to soring objects array no need to update metadata */
   uint32_t num = MAX_PKT_BURST;
   num = rte_soring_enqueue_burst(soring, pkts, num, NULL);
   ....
   /* dequeux - reads both packets and related tx_state */
   uint32_t num = MAX_PKT_BURST;
   num = rte_soring_dequeux_burst(soring, pkts, txst, num, NULL);

   /*
    * Tx packets out, or drop in case of error.
    * Note that we don't need to dereference the soring objects itself
    * to make a decision.
    */
   uint32_t i, j, k, n;
   struct rte_mbuf *dr[MAX_PKT_BURST];

   k = 0;
   for (i = 0; i != num; i++) {
       /* packet processing reports an error */
       if (txst[i].rc < 0)
           dr[k++] = pkts[i];
       /* valid packet, send it out */
       else {
           /* group consecutive packets with the same port and queue IDs */
           for (j = i + 1; j < num; j++)
               if (txst[j].rc != txst[i].rc)
                   break;

           n = rte_eth_tx_burst(txst[i].tx.port_id, txst[i].tx.queue_id,
                                pkts + i, j - i);
           if (i + n != j) {
               /* decide with unsent packets if any */
           }
       }
   }
   /* drop erroneous packets */
   if (k != 0)
       rte_pktmbuf_free_bulk(dr, k);

acquire/release part:

.. code-block:: c

   uint32_t ftoken;
   struct rte_mbuf *pkts[MAX_PKT_BURST];
   union tx_state txst[MAX_PKT_BURST];
   ...
   /* acquire - grab some packets to process */
   uint32_t num = MAX_PKT_BURST;
   num = rte_soring_acquire_burst(soring, pkts, 0, num, &ftoken, NULL);

   /* process packets, fill txst[] for each */
   do_process_packets(pkts, txst, num);

   /*
    * release - assuming that do_process_packets() didn't change
    * contents of pkts[], we need to update soring metadata array only.
    */
   rte_soring_releasx(soring, NULL, txst, 0, num, ftoken);

Use Cases
~~~~~~~~~

Expected use-cases include applications that use pipeline model
(probably with multiple stages) for packet processing,
when preserving incoming packet order is important.
I.E.: IPsec processing, etc.

SORING internals
~~~~~~~~~~~~~~~~

* In addition to accessible by the user array of objects (and metadata),
  ``soring`` also contains an internal array of states.
  Each ``state[]`` corresponds to exactly one object within the soring.
  That ``state[]`` array is used by ``acquire``/``release``/``dequeue`` operations
  to store internal information and should not be accessed by the user directly.

* At ``acquire``, soring  moves stage's head
  (in a same way as ``rte_ring`` ``move_head`` does),
  plus it saves in ``state[stage.old_head]`` information
  about how many elements were acquired, acquired head position,
  and special flag value to indicate that given elements are acquired
  (``SORING_ST_START``).
  Note that ``acquire`` returns an opaque ``ftoken`` value
  that user has to provide for ``release`` function.

* ``release`` extracts old head value from provided by user ``ftoken``
  and checks that corresponding ``state[]`` entry contains expected values
  (mostly for sanity purposes).
  Then it marks this ``state[]`` entry with ``SORING_ST_FINISH`` flag
  to indicate that given subset of objects was released.
  After that, it checks does stage's old ``head`` value
  equals to its current ``tail`` value.
  If so, then it performs ``finalize`` operation,
  otherwise ``release`` just returns.

* As ``state[]`` is shared by all threads,
  some other thread can perform ``finalize`` operation for given stage.
  That allows ``release`` to avoid excessive waits on the ``tail`` value.
  Main purpose of ``finalize`` operation is to walk through ``state[]`` array
  from current stage's ``tail`` position up to its ``head``,
  check ``state[]`` and move stage ``tail`` through elements
  that are already released (in ``SORING_ST_FINISH`` state).
  Along with that, corresponding ``state[]`` entries are reset back to zero.
  Note that ``finalize`` for given stage can be called from multiple places:
  from ``release`` for that stage or from ``acquire`` for next stage,
  or even from consumer's ``dequeue`` - in case given stage is the last one.
  So ``finalize`` has to be MT-safe and inside it we have to guarantee that
  at any given moment only one thread can update stage's ``tail``
  and reset corresponding ``state[]`` entries.


References
----------

    *   `bufring.h in FreeBSD <http://svn.freebsd.org/viewvc/base/release/8.0.0/sys/sys/buf_ring.h?revision=199625&amp;view=markup>`_ (version 8)

    *   `bufring.c in FreeBSD <http://svn.freebsd.org/viewvc/base/release/8.0.0/sys/kern/subr_bufring.c?revision=199625&amp;view=markup>`_ (version 8)

    *   `Linux Lockless Ring Buffer Design <http://lwn.net/Articles/340400/>`_
