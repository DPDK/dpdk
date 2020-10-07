..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

Stack Mempool Driver
====================

**rte_mempool_stack** is a pure software mempool driver based on the
``rte_stack`` DPDK library. For run-to-completion workloads with sufficiently
large per-lcore caches, the mbufs will likely stay in the per-lcore caches and
the mempool type (ring, stack, etc.) will have a negligible impact on
performance. However a stack-based mempool is often better suited to pipelined
packet-processing workloads (which allocate and free mbufs on different lcores)
than a ring-based mempool, since its LIFO behavior results in better temporal
locality and a minimal memory footprint even if the mempool is
over-provisioned. Users are encouraged to benchmark with multiple mempool types
to determine which works best for their specific application.

The following modes of operation are available for the stack mempool driver and
can be selected as described in :ref:`Mempool_Handlers`:

- ``stack``

  The underlying **rte_stack** operates in standard (lock-based) mode.
  For more information please refer to :ref:`Stack_Library_Std_Stack`.

- ``lf_stack``

  The underlying **rte_stack** operates in lock-free mode. For more
  information please refer to :ref:`Stack_Library_LF_Stack`.

The standard stack outperforms the lock-free stack on average, however the
standard stack is non-preemptive: if a mempool user is preempted while holding
the stack lock, that thread will block all other mempool accesses until it
returns and releases the lock. As a result, an application using the standard
stack whose threads can be preempted can suffer from brief, infrequent
performance hiccups.

The lock-free stack, by design, is not susceptible to this problem; one thread can
be preempted at any point during a push or pop operation and will not impede
the progress of any other thread.

For a more detailed description of the stack implementations, please refer to
:doc:`../prog_guide/stack_lib`.
