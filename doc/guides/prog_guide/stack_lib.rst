..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Stack Library
=============

DPDK's stack library provides an API for configuration and use of a bounded
stack of pointers.

The stack library provides the following basic operations:

*  Create a uniquely named stack of a user-specified size and using a
   user-specified socket.

*  Push and pop a burst of one or more stack objects (pointers). These function
   are multi-threading safe.

*  Free a previously created stack.

*  Lookup a pointer to a stack by its name.

*  Query a stack's current depth and number of free entries.

Implementation
~~~~~~~~~~~~~~

The stack consists of a contiguous array of pointers, a current index, and a
spinlock. Accesses to the stack are made multi-thread safe by the spinlock.
