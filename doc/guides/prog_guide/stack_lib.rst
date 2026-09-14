..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.
    Copyright(c) 2026 SmartShare Systems.

Stack Library
=============

DPDK's stack library provides an API for configuration and use of a bounded
stack of pointers.

The stack library provides the following basic operations:

*  Create a uniquely named stack (or pile) of a user-specified size and using a
   user-specified socket, with either standard (lock-based) or lock-free
   behavior.
   The pile resembles a lock-free stack, but is not strictly LIFO.

*  Push and pop a burst of one or more stack objects (pointers).
   These functions are multi-thread safe.

*  Free a previously created stack.

*  Lookup a pointer to a stack by its name.

*  Query a stack's current depth and number of free entries.

Implementation
--------------

The library supports three types of stacks: standard (lock-based), lock-free,
and pile (lock-free, not strictly LIFO, optimized for bulk operations).
All types use the same set of interfaces, but their implementations differ.

.. _Stack_Library_Std_Stack:

Lock-based stack
~~~~~~~~~~~~~~~~

The lock-based stack consists of a contiguous array of pointers, a current
index, and a spinlock. Accesses to the stack are made multi-thread safe by the
spinlock.

.. _Stack_Library_LF_Stack:

Lock-free stack
~~~~~~~~~~~~~~~

The lock-free stack consists of a linked list of elements, each containing a
data pointer and a next pointer, and an atomic stack depth counter. The
lock-free property means that multiple threads can push and pop simultaneously.
One thread being preempted/delayed in a push or pop operation will not
impede the forward progress of any other thread.

The lock-free push operation enqueues a linked list of pointers by pointing the
list's tail to the current stack head, and using a CAS to swing the stack head
pointer to the head of the list. The operation retries if it is unsuccessful
(i.e. the list changed between reading the head and modifying it), else it
adjusts the stack length and returns.

The lock-free pop operation first reserves one or more list elements by
adjusting the stack length, to ensure the dequeue operation will succeed
without blocking. It then dequeues pointers by walking the list -- starting
from the head -- then swinging the head pointer (using a CAS as well). While
walking the list, the data pointers are recorded in an object table.

The linked list elements themselves are maintained in a lock-free LIFO, and are
allocated before stack pushes and freed after stack pops. Since the stack has a
fixed maximum depth, these elements do not need to be dynamically created.

The lock-free behavior is selected by passing the ``RTE_STACK_F_LF`` flag to
``rte_stack_create()``.

Preventing the ABA problem
^^^^^^^^^^^^^^^^^^^^^^^^^^

To prevent the ABA problem, this algorithm uses a 128-bit compare-and-swap instruction
to atomically update both the stack top pointer and a modification counter.
The ABA problem can occur without a modification counter if, for example:

#. Thread A reads head pointer X and stores the pointed-to list element.

#. Other threads modify the list such that the head pointer is once again X,
   but its pointed-to data is different than what thread A read.

#. Thread A changes the head pointer with a compare-and-swap and succeeds.

In this case thread A would not detect that the list had changed, and would
both pop stale data and incorrectly change the head pointer. By adding a
modification counter that is updated on every push and pop as part of the
compare-and-swap, the algorithm can detect when the list changes even if the
head pointer remains the same.

.. _Stack_Library_Pile:

Pile
~~~~

The pile is a stack-like implementation, optimized for bulk operations.
It is only LIFO on bulk level, not on object level;
i.e. arrays of bulks are pushed and popped in LIFO manner,
but objects within each bulk are not ordered as expected by a stack.
Furthermore, it is not strictly bounded by its size, but might hold more objects.

The pile implementation generally resembles that of the lock-free stack.
In addition to the lock-free stack's linked list of solo (single-object) elements,
it also contains a linked list of bulk (multi-object) elements.
And similar to the linked list of free elements,
it contains two linked lists of free elements,
one for each element type (bulk and solo).
The lock-free property means that multiple threads can push and pop simultaneously.
One thread being preempted/delayed in a push or pop operation
will not impede the forward progress of any other thread.

Push operations are performed by splitting the burst in two:
objects fitting into bulk elements,
and any remaining objects (after filling bulk elements) into solo elements,
and then performing two lock-free push operations,
one for each element type (solo and bulk).

Pop operations are performed by splitting the burst in two:
objects fitting into bulk elements,
and any remaining objects (not filling a bulk element) into solo elements.
Two lock-free pop operations are performed,
first for bulk elements, and then for solo elements.
If the pop operation for bulk elements fails,
it keeps retrying, requesting one less bulk element.
The number of solo elements in the following request is correspondingly increased.

The pile's lock-free list push and pop operations
use the lock-free stack's implementations
(and uses type casting to mimic C++ class inheritance).

The linked list elements themselves are maintained in two lock-free LIFOs,
one for bulk elements and one for solo elements,
and are allocated before pushes and freed after pops.
Since the pile has a fixed maximum depth,
these elements do not need to be dynamically created.

The pile behavior is selected
by passing the ``RTE_STACK_F_PILE`` flag to ``rte_stack_create()``.

The pile bulk size can be changed
by modifying ``RTE_STACK_PILE_BULK_SIZE`` in ``config/rte_config.h``.
For optimal performance when using the pile mempool driver,
the mempool cache size / 2 should be divisible by the pile bulk size.

.. note::

   The pile is designed and optimized for use with bulks of objects.
   Bursts not a multiple of the bulk size are still handled in a lock-free,
   forward-progress-guaranteed manner.
   However, pop operations may exhibit significantly lower performance
   in instances where the optimal number of bulk elements is unavailable,
   and it is necessary to retry
   (fetching increasingly fewer bulk elements and correspondingly more solo elements).
