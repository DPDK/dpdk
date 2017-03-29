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

.. _Ring_Library:

Ring Library
============

The ring allows the management of queues.
Instead of having a linked list of infinite size, the rte_ring has the following properties:

*   FIFO

*   Maximum size is fixed, the pointers are stored in a table

*   Lockless implementation

*   Multi-consumer or single-consumer dequeue

*   Multi-producer or single-producer enqueue

*   Bulk dequeue - Dequeues the specified count of objects if successful; otherwise fails

*   Bulk enqueue - Enqueues the specified count of objects if successful; otherwise fails

*   Burst dequeue - Dequeue the maximum available objects if the specified count cannot be fulfilled

*   Burst enqueue - Enqueue the maximum available objects if the specified count cannot be fulfilled

The advantages of this data structure over a linked list queue are as follows:

*   Faster; only requires a single Compare-And-Swap instruction of sizeof(void \*) instead of several double-Compare-And-Swap instructions.

*   Simpler than a full lockless queue.

*   Adapted to bulk enqueue/dequeue operations.
    As pointers are stored in a table, a dequeue of several objects will not produce as many cache misses as in a linked queue.
    Also, a bulk dequeue of many objects does not cost more than a dequeue of a simple object.

The disadvantages:

*   Size is fixed

*   Having many rings costs more in terms of memory than a linked list queue. An empty ring contains at least N pointers.

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

A pointer to the added object is copied in the ring (obj4).


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

   Dequeue last step


Dequeue Second Step
^^^^^^^^^^^^^^^^^^^

The second step is to modify ring->cons_head in the ring structure to point to the same location as cons_next.

The pointer to the dequeued object (obj1) is copied in the pointer given by the user.


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
The indexes are between 0 and 2^32 -1, and we mask their value when we access the pointer table (the ring itself).
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

References
----------

    *   `bufring.h in FreeBSD <http://svn.freebsd.org/viewvc/base/release/8.0.0/sys/sys/buf_ring.h?revision=199625&amp;view=markup>`_ (version 8)

    *   `bufring.c in FreeBSD <http://svn.freebsd.org/viewvc/base/release/8.0.0/sys/kern/subr_bufring.c?revision=199625&amp;view=markup>`_ (version 8)

    *   `Linux Lockless Ring Buffer Design <http://lwn.net/Articles/340400/>`_
