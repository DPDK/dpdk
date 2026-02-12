.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 Ericsson AB

Lcore Variables
===============

The ``rte_lcore_var.h`` API provides a mechanism to allocate and
access per-lcore id variables in a space- and cycle-efficient manner.


Lcore Variables API
-------------------

A per-lcore id variable (or lcore variable for short)
holds a unique value for each EAL thread and registered non-EAL thread.
Thus, there is one distinct value for each past, current and future
lcore id-equipped thread, with a total of ``RTE_MAX_LCORE`` instances.

The value of the lcore variable for one lcore id is independent of the
values associated with other lcore ids within the same variable.

For detailed information on the lcore variables API,
please refer to the ``rte_lcore_var.h`` API documentation.


Lcore Variable Handle
~~~~~~~~~~~~~~~~~~~~~

To allocate and access an lcore variable's values, a *handle* is used.
The handle is represented by an opaque pointer,
only to be dereferenced using the appropriate ``<rte_lcore_var.h>`` macros.

The handle is a pointer to the value's type
(e.g., for an ``uint32_t`` lcore variable, the handle is a ``uint32_t *``).

The reason the handle is typed (i.e., it's not a void pointer or an integer)
is to enable type checking when accessing values of the lcore variable.

A handle may be passed between modules and threads
just like any other pointer.

A valid (i.e., allocated) handle never has the value NULL.
Thus, a handle set to NULL may be used
to signify that allocation has not yet been done.


Lcore Variable Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~

An lcore variable is created in two steps:

1. Define an lcore variable handle by using ``RTE_LCORE_VAR_HANDLE``.
2. Allocate lcore variable storage and initialize the handle
   by using ``RTE_LCORE_VAR_ALLOC`` or ``RTE_LCORE_VAR_INIT``.
   Allocation generally occurs at the time of module initialization,
   but may be done at any time.

The lifetime of an lcore variable is not tied to the thread that created it.

Each lcore variable has ``RTE_MAX_LCORE`` values,
one for each possible lcore id.
All of an lcore variable's values may be accessed
from the moment the lcore variable is created,
throughout the lifetime of the EAL (i.e., until ``rte_eal_cleanup()``).

Lcore variables do not need to be freed and cannot be freed.


Access
~~~~~~

The value of any lcore variable for any lcore id
may be accessed from any thread (including unregistered threads),
but it should only be *frequently* read from or written to by the *owner*.
A thread is considered the owner of a particular lcore variable value instance
if it has the lcore id associated with that instance.

Non-owner accesses results in *false sharing*.
As long as non-owner accesses are rare,
they will have only a very slight effect on performance.
This property of lcore variables memory organization is intentional.
See the implementation section for more information.

Values of the same lcore variable,
associated with different lcore ids may be frequently read or written
by their respective owners without risking false sharing.

An appropriate synchronization mechanism,
such as atomic load and stores,
should be employed to prevent data races between the owning thread
and any other thread accessing the same value instance.

The value of the lcore variable for a particular lcore id
is accessed via ``RTE_LCORE_VAR_LCORE``.

A common pattern is for an EAL thread or a registered non-EAL thread
to access its own lcore variable value.
For this purpose, a shorthand exists as ``RTE_LCORE_VAR``.

``RTE_LCORE_VAR_FOREACH`` may be used to iterate
over all values of a particular lcore variable.

The handle, defined by ``RTE_LCORE_VAR_HANDLE``,
is a pointer of the same type as the value,
but it must be treated as an opaque identifier
and cannot be directly dereferenced.

Lcore variable handles and value pointers may be freely passed
between different threads.


Storage
~~~~~~~

An lcore variable's values may be of a primitive type like ``int``,
but is typically a ``struct``.

The lcore variable handle introduces a per-variable
(not per-value/per-lcore id) overhead of ``sizeof(void *)`` bytes,
so there are some memory footprint gains to be made by organizing
all per-lcore id data for a particular module as one lcore variable
(e.g., as a struct).

An application may define an lcore variable handle
without ever allocating the lcore variable.

The size of an lcore variable's value cannot exceed
the DPDK build-time constant ``RTE_MAX_LCORE_VAR``.
An lcore variable's size is the size of one of its value instance,
not the aggregate of all its ``RTE_MAX_LCORE`` instances.

Lcore variables should generally *not* be ``__rte_cache_aligned``
and need *not* include a ``RTE_CACHE_GUARD`` field,
since these constructs are designed to avoid false sharing.
With lcore variables, false sharing is largely avoided by other means.
In the case of an lcore variable instance,
the thread most recently accessing nearby data structures
should almost always be the lcore variable's owner.
Adding padding (e.g., with ``RTE_CACHE_GUARD``)
will increase the effective memory working set size,
potentially reducing performance.

Lcore variable values are initialized to zero by default.

Lcore variables are not stored in huge page memory.


Example
~~~~~~~

Below is an example of the use of an lcore variable:

.. code-block:: c

   struct foo_lcore_state {
           int a;
           long b;
   };

   static RTE_LCORE_VAR_HANDLE(struct foo_lcore_state, lcore_states);

   long foo_get_a_plus_b(void)
   {
           const struct foo_lcore_state *state = RTE_LCORE_VAR(lcore_states);

           return state->a + state->b;
   }

   RTE_INIT(rte_foo_init)
   {
           RTE_LCORE_VAR_ALLOC(lcore_states);

           unsigned int lcore_id;
           struct foo_lcore_state *state;
           RTE_LCORE_VAR_FOREACH(lcore_id, state, lcore_states) {
                   /* initialize state */
           }

           /* other initialization */
   }


Implementation
--------------

This section gives an overview of the implementation of lcore variables,
and some background to its design.


Lcore Variable Buffers
~~~~~~~~~~~~~~~~~~~~~~

Lcore variable values are kept in a set of ``lcore_var_buffer`` structs.

.. literalinclude:: ../../../lib/eal/common/eal_common_lcore_var.c
   :language: c
   :start-after: base unit
   :end-before: last allocated unit

An lcore var buffer stores at a minimum one, but usually many, lcore variables.

The value instances for all lcore ids are stored in the same buffer.
However, each lcore id has its own slice of the ``data`` array.
Such a slice is ``RTE_MAX_LCORE_VAR`` bytes in size.

In this way, the values associated with a particular lcore id
are grouped spatially close (in memory).
No padding is required to prevent false sharing.

.. literalinclude:: ../../../lib/eal/common/eal_common_lcore_var.c
   :language: c
   :start-after: last allocated unit
   :end-before: >8 end of documented variables

The implementation maintains a current ``lcore_var_buffer`` and an ``offset``,
where the latter tracks how many bytes of this current buffer has been allocated.

The ``offset`` is progressively incremented
(by the size of the just-allocated lcore variable),
as lcore variables are being allocated.

If the allocation of a variable would result in an ``offset`` larger
than ``RTE_MAX_LCORE_VAR`` (i.e., the slice size), the buffer is full.
In that case, new buffer is allocated off the heap, and the ``offset`` is reset.

The lcore var buffers are arranged in a link list,
to allow freeing them at the point of ``rte_eal_cleanup()``.

The lcore variable buffers are allocated off the regular C heap.
There are a number of reasons for not using ``<rte_malloc.h>``
and huge pages for lcore variables:

- The libc heap is available at any time,
  including early in the DPDK initialization.
- The amount of data kept in lcore variables is projected to be small,
  and thus is unlikely to induce translate lookaside buffer (TLB) misses.
- The last (and potentially only) lcore buffer in the chain
  will likely only partially be in use.
  Huge pages of the sort used by DPDK are always resident in memory,
  and their use would result in a significant amount of memory going to waste.
  An example: ~256 kB worth of lcore variables are allocated
  by DPDK libraries, PMDs and the application.
  ``RTE_MAX_LCORE_VAR`` is set to 128 kB and ``RTE_MAX_LCORE`` to 128.
  With 4 kB OS pages, only the first ~64 pages of each of the 128 per-lcore id slices
  in the (only) ``lcore_var_buffer`` will actually be resident (paged in).
  Here, demand paging saves ~98 MB of memory.

.. note::

   Not residing in huge pages, lcore variables cannot be accessed from secondary processes.

Heap allocation failures are treated as fatal.
The reason for this unorthodox design is that a majority of the allocations
are deemed to happen at initialization.
An early heap allocation failure for a fixed amount of data is a situation
not unlike one where there is not enough memory available for static variables
(i.e., the BSS or data sections).

Provided these assumptions hold true, it's deemed acceptable
to leave the application out of handling memory allocation failures.

The upside of this approach is that no error handling code is required
on the API user side.


Lcore Variable Handles
~~~~~~~~~~~~~~~~~~~~~~

Upon lcore variable allocation, the lcore variables API returns
an opaque *handle* in the form of a pointer.
The value of the pointer is ``buffer->data + offset``.

Translating a handle base pointer to a pointer to a value
associated with a particular lcore id is straightforward:

.. literalinclude:: ../../../lib/eal/include/rte_lcore_var.h
   :language: c
   :start-after: access function 8<
   :end-before: >8 end of access function

``RTE_MAX_LCORE_VAR`` is a public macro to allow the compiler
to optimize the ``lcore_id * RTE_MAX_LCORE_VAR`` expression,
and replace the multiplication with a less expensive arithmetic operation.

To maintain type safety, the ``RTE_LCORE_VAR*()`` macros should be used,
instead of directly invoking ``rte_lcore_var_lcore()``.
The macros return a pointer of the same type as the handle
(i.e., a pointer to the value's type).


Memory Layout
~~~~~~~~~~~~~

This section describes how lcore variables are organized in memory.

As an illustration, two example modules are used,
``rte_x`` and ``rte_y``, both maintaining per-lcore id state
as a part of their implementation.

Two different methods will be used to maintain such state -
lcore variables and, to serve as a reference, lcore id-indexed static arrays.

Certain parameters are scaled down to make graphical depictions more practical.

For the purpose of this exercise, a ``RTE_MAX_LCORE`` of 2 is assumed.
In a real-world configuration, the maximum number of
EAL threads and registered threads will be much greater (e.g., 128).

The lcore variables example assumes a ``RTE_MAX_LCORE_VAR`` of 64.
In a real-world configuration (as controlled by ``rte_config.h``),
the value of this compile-time constant will be much greater (e.g., 1048576).

The per-lcore id state is also smaller than what most real-world modules would have.

Lcore Variables Example
^^^^^^^^^^^^^^^^^^^^^^^

When lcore variables are used, the parts of ``rte_x`` and ``rte_y``
that deal with the declaration and allocation of per-lcore id data
may look something like below.

.. code-block:: c

   /* -- Lcore variables -- */

   /* rte_x.c */

   struct x_lcore
   {
       int a;
       char b;
   };

   static RTE_LCORE_VAR_HANDLE(struct x_lcore, x_lcores);
   RTE_LCORE_VAR_INIT(x_lcores);

   /../

   /* rte_y.c */

   struct y_lcore
   {
       long c;
       long d;
   };

   static RTE_LCORE_VAR_HANDLE(struct y_lcore, y_lcores);
   RTE_LCORE_VAR_INIT(y_lcores);

   /../

The resulting memory layout will look something like the following:

.. figure:: img/lcore_var_mem_layout.*

The above figure assumes that ``x_lcores`` is allocated prior to ``y_lcores``.
``RTE_LCORE_VAR_INIT()`` relies constructors, run prior to ``main()`` in an undefined order.

The use of lcore variables ensures that per-lcore id data is kept in close proximity,
within a designated region of memory.
This proximity enhances data locality and can improve performance.

Lcore Id Index Static Array Example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Below is an example of the struct declarations,
declarations and the resulting organization in memory
in case an lcore id indexed static array of cache-line aligned,
RTE_CACHE_GUARDed structs are used to maintain per-lcore id state.

This is a common pattern in DPDK, which lcore variables attempts to replace.

.. code-block:: c

   /* -- Cache-aligned static arrays -- */

   /* rte_x.c */

   struct __rte_cache_aligned x_lcore
   {
       int a;
       char b;
       RTE_CACHE_GUARD;
   };

   static struct x_lcore x_lcores[RTE_MAX_LCORE];

   /../

   /* rte_y.c */

   struct __rte_cache_aligned y_lcore
   {
       long c;
       long d;
       RTE_CACHE_GUARD;
   };

   static struct y_lcore y_lcores[RTE_MAX_LCORE];

   /../

In this approach, accessing the state for a particular lcore id is merely
a matter retrieving the lcore id and looking up the correct struct instance.

.. code-block:: c

   struct x_lcore *my_lcore_state = &x_lcores[rte_lcore_id()];

The address "0" at the top of the left-most column in the figure
represent the base address for the ``x_lcores`` array
(in the BSS segment in memory).

The figure only includes the memory layout for the ``rte_x`` example module.
``rte_y`` would look very similar, with ``y_lcores`` being located
at some other address in the BSS section.

.. figure:: img/static_array_mem_layout.*

The static array approach results in the per-lcore id
being organized around modules, not lcore ids.
To avoid false sharing, an extensive use of padding is employed,
causing cache fragmentation.

Because the padding is interspersed with the data,
demand paging is unlikely to reduce the actual resident DRAM memory footprint.
This is because the padding is smaller
than a typical operating system memory page (usually 4 kB).


Performance
~~~~~~~~~~~

One of the goals of lcore variables is to improve performance.
This is achieved by packing often-used data in fewer cache lines,
and thus reducing fragmentation in CPU caches
and thus somewhat improving the effective cache size and cache hit rates.

The application-level gains depends much on how much data is kept in lcore variables,
and how often it is accessed,
and how much pressure the application asserts on the CPU caches
(i.e., how much other memory it accesses).

The ``lcore_var_perf_autotest`` is an attempt at exploring
the performance benefits (or drawbacks) of lcore variables
compared to its alternatives.
Being a micro benchmark, it needs to be taken with a grain of salt.

Generally, one shouldn't expect more than some very modest gains in performance
after a switch from lcore id indexed arrays to lcore variables.

An additional benefit of the use of lcore variables is that it avoids
certain tricky issues related to CPU core hardware prefetching
(e.g., next-N-lines prefetching) that may cause false sharing
even when data used by two cores do not reside on the same cache line.
Hardware prefetch behavior is generally not publicly documented
and varies across CPU vendors, CPU generations and BIOS (or similar) configurations.
For applications aiming to be portable, this may cause issues.
Often, CPU hardware prefetch-induced issues are non-existent,
except some particular circumstances, where their adverse effects may be significant.


Alternatives
------------

Lcore Id Indexed Static Arrays
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Lcore variables are designed to replace a pattern exemplified below:

.. code-block:: c

   struct __rte_cache_aligned foo_lcore_state {
           int a;
           long b;
           RTE_CACHE_GUARD;
   };

   static struct foo_lcore_state lcore_states[RTE_MAX_LCORE];

This scheme is simple and effective, but has one drawback:
the data is organized so that objects related to all lcores for a particular module
are kept close in memory.
At a bare minimum, this requires sizing data structures
(e.g., using ``__rte_cache_aligned``) to an even number of cache lines
and ensuring that allocation of such objects
are cache line aligned to avoid false sharing.
With CPU hardware prefetching and memory loads resulting from speculative execution
(functions which seemingly are getting more eager faster
than they are getting more intelligent),
one or more "guard" cache lines may be required
to separate one lcore's data from another's and prevent false sharing.

Lcore variables offer the advantage of working with,
rather than against, the CPU's assumptions.
A next-line hardware prefetcher, for example, may function as intended
(i.e., to the benefit, not detriment, of system performance).


Thread Local Storage
~~~~~~~~~~~~~~~~~~~~

An alternative to ``rte_lcore_var.h`` is the ``rte_per_lcore.h`` API,
which makes use of thread-local storage
(TLS, e.g., GCC ``__thread`` or C11 ``_Thread_local``).

There are a number of differences between using TLS
and the use of lcore variables.

The lifecycle of a thread-local variable instance is tied to that of the thread.
The data cannot be accessed before the thread has been created,
nor after it has terminated.
As a result, thread-local variables must be initialized in a "lazy" manner
(e.g., at the point of thread creation).
Lcore variables may be accessed immediately after having been allocated
(which may occur before any thread beyond the main thread is running).

A thread-local variable is duplicated across all threads in the process,
including unregistered non-EAL threads (i.e., "regular" threads).
For DPDK applications heavily relying on multi-threading
(in conjunction to DPDK's "one thread per core" pattern),
either by having many concurrent threads or creating/destroying threads at a high rate,
an excessive use of thread-local variables may cause inefficiencies
(e.g., increased thread creation overhead due to thread-local storage initialization
or increased memory footprint).
Lcore variables *only* exist for threads with an lcore id.

Whether data in thread-local storage can be shared between threads
(i.e., whether a pointer to a thread-local variable can be passed to
and successfully dereferenced by a non-owning thread)
depends on the specifics of the TLS implementation.
With GCC ``__thread`` and GCC ``_Thread_local``,
data sharing between threads is supported.
In the C11 standard, accessing another thread's ``_Thread_local`` object
is implementation-defined.
Lcore variable instances may be accessed reliably by any thread.

Lcore variables also relies on TLS to retrieve the thread's lcore id.
However, the rest of the per-thread data is not kept in TLS.

From a memory layout perspective, TLS is similar to lcore variables,
and thus per-thread data structure need not be padded.

In case the above-mentioned drawbacks of the use of TLS is of no significance
to a particular application, TLS is a good alternative to lcore variables.
