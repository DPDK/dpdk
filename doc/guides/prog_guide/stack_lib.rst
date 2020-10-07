..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Stack Library
=============

DPDK's stack library provides an API for configuration and use of a bounded
stack of pointers.

The stack library provides the following basic operations:

*  Create a uniquely named stack of a user-specified size and using a
   user-specified socket, with either standard (lock-based) or lock-free
   behavior.

*  Push and pop a burst of one or more stack objects (pointers). These function
   are multi-threading safe.

*  Free a previously created stack.

*  Lookup a pointer to a stack by its name.

*  Query a stack's current depth and number of free entries.

Implementation
~~~~~~~~~~~~~~

The library supports two types of stacks: standard (lock-based) and lock-free.
Both types use the same set of interfaces, but their implementations differ.

.. _Stack_Library_Std_Stack:

Lock-based Stack
----------------

The lock-based stack consists of a contiguous array of pointers, a current
index, and a spinlock. Accesses to the stack are made multi-thread safe by the
spinlock.

.. _Stack_Library_LF_Stack:

Lock-free Stack
------------------

The lock-free stack consists of a linked list of elements, each containing a
data pointer and a next pointer, and an atomic stack depth counter. The
lock-free property means that multiple threads can push and pop simultaneously,
and one thread being preempted/delayed in a push or pop operation will not
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

The lock-free behavior is selected by passing the *RTE_STACK_F_LF* flag to
rte_stack_create().

Preventing the ABA Problem
^^^^^^^^^^^^^^^^^^^^^^^^^^

To prevent the ABA problem, this algorithm stack uses a 128-bit
compare-and-swap instruction to atomically update both the stack top pointer
and a modification counter. The ABA problem can occur without a modification
counter if, for example:

1. Thread A reads head pointer X and stores the pointed-to list element.
2. Other threads modify the list such that the head pointer is once again X,
   but its pointed-to data is different than what thread A read.
3. Thread A changes the head pointer with a compare-and-swap and succeeds.

In this case thread A would not detect that the list had changed, and would
both pop stale data and incorrect change the head pointer. By adding a
modification counter that is updated on every push and pop as part of the
compare-and-swap, the algorithm can detect when the list changes even if the
head pointer remains the same.
