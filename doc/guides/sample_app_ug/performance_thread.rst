..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Performance Thread Sample Application
=====================================

The performance thread sample application is a derivative of the standard L3
forwarding application that demonstrates different threading models.

Overview
--------
For a general description of the L3 forwarding applications capabilities
please refer to the documentation of the standard application in
:doc:`l3_forward`.

The performance thread sample application differs from the standard L3
forwarding example in that it divides the TX and RX processing between
different threads, and makes it possible to assign individual threads to
different cores.

Three threading models are considered:

#. When there is one EAL thread per physical core.
#. When there are multiple EAL threads per physical core.
#. When there are multiple lightweight threads per EAL thread.

Since DPDK release 2.0 it is possible to launch applications using the
``--lcores`` EAL parameter, specifying cpu-sets for a physical core. With the
performance thread sample application its is now also possible to assign
individual RX and TX functions to different cores.

As an alternative to dividing the L3 forwarding work between different EAL
threads the performance thread sample introduces the possibility to run the
application threads as lightweight threads (L-threads) within one or
more EAL threads.

In order to facilitate this threading model the example includes a primitive
cooperative scheduler (L-thread) subsystem. More details of the L-thread
subsystem can be found in :ref:`lthread_subsystem`.

**Note:** Whilst theoretically possible it is not anticipated that multiple
L-thread schedulers would be run on the same physical core, this mode of
operation should not be expected to yield useful performance and is considered
invalid.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the `performance-thread/l3fwd-thread` sub-directory.

Running the Application
-----------------------

The application has a number of command line options::

    ./<build_dir>/examples/dpdk-l3fwd-thread [EAL options] --
        -p PORTMASK [-P]
        --rx(port,queue,lcore,thread)[,(port,queue,lcore,thread)]
        --tx(lcore,thread)[,(lcore,thread)]
        [--max-pkt-len PKTLEN]  [--no-numa]
        [--hash-entry-num] [--ipv6] [--no-lthreads] [--stat-lcore lcore]
        [--parse-ptype]

Where:

* ``-p PORTMASK``: Hexadecimal bitmask of ports to configure.

* ``-P``: optional, sets all ports to promiscuous mode so that packets are
  accepted regardless of the packet's Ethernet MAC destination address.
  Without this option, only packets with the Ethernet MAC destination address
  set to the Ethernet address of the port are accepted.

* ``--rx (port,queue,lcore,thread)[,(port,queue,lcore,thread)]``: the list of
  NIC RX ports and queues handled by the RX lcores and threads. The parameters
  are explained below.

* ``--tx (lcore,thread)[,(lcore,thread)]``: the list of TX threads identifying
  the lcore the thread runs on, and the id of RX thread with which it is
  associated. The parameters are explained below.

* ``--max-pkt-len``: optional, maximum packet length in decimal (64-9600).

* ``--no-numa``: optional, disables numa awareness.

* ``--hash-entry-num``: optional, specifies the hash entry number in hex to be
  setup.

* ``--ipv6``: optional, set it if running ipv6 packets.

* ``--no-lthreads``: optional, disables l-thread model and uses EAL threading
  model. See below.

* ``--stat-lcore``: optional, run CPU load stats collector on the specified
  lcore.

* ``--parse-ptype:`` optional, set to use software to analyze packet type.
  Without this option, hardware will check the packet type.

The parameters of the ``--rx`` and ``--tx`` options are:

* ``--rx`` parameters

   .. _table_l3fwd_rx_parameters:

   +--------+------------------------------------------------------+
   | port   | RX port                                              |
   +--------+------------------------------------------------------+
   | queue  | RX queue that will be read on the specified RX port  |
   +--------+------------------------------------------------------+
   | lcore  | Core to use for the thread                           |
   +--------+------------------------------------------------------+
   | thread | Thread id (continuously from 0 to N)                 |
   +--------+------------------------------------------------------+


* ``--tx`` parameters

   .. _table_l3fwd_tx_parameters:

   +--------+------------------------------------------------------+
   | lcore  | Core to use for L3 route match and transmit          |
   +--------+------------------------------------------------------+
   | thread | Id of RX thread to be associated with this TX thread |
   +--------+------------------------------------------------------+

The ``l3fwd-thread`` application allows you to start packet processing in two
threading models: L-Threads (default) and EAL Threads (when the
``--no-lthreads`` parameter is used). For consistency all parameters are used
in the same way for both models.


Running with L-threads
~~~~~~~~~~~~~~~~~~~~~~

When the L-thread model is used (default option), lcore and thread parameters
in ``--rx/--tx`` are used to affinitize threads to the selected scheduler.

For example, the following places every l-thread on different lcores::

   dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                --rx="(0,0,0,0)(1,0,1,1)" \
                --tx="(2,0)(3,1)"

The following places RX l-threads on lcore 0 and TX l-threads on lcore 1 and 2
and so on::

   dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                --rx="(0,0,0,0)(1,0,0,1)" \
                --tx="(1,0)(2,1)"


Running with EAL threads
~~~~~~~~~~~~~~~~~~~~~~~~

When the ``--no-lthreads`` parameter is used, the L-threading model is turned
off and EAL threads are used for all processing. EAL threads are enumerated in
the same way as L-threads, but the ``--lcores`` EAL parameter is used to
affinitize threads to the selected cpu-set (scheduler). Thus it is possible to
place every RX and TX thread on different lcores.

For example, the following places every EAL thread on different lcores::

   dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                --rx="(0,0,0,0)(1,0,1,1)" \
                --tx="(2,0)(3,1)" \
                --no-lthreads


To affinitize two or more EAL threads to one cpu-set, the EAL ``--lcores``
parameter is used.

The following places RX EAL threads on lcore 0 and TX EAL threads on lcore 1
and 2 and so on::

   dpdk-l3fwd-thread -l 0-7 -n 2 --lcores="(0,1)@0,(2,3)@1" -- -P -p 3 \
                --rx="(0,0,0,0)(1,0,1,1)" \
                --tx="(2,0)(3,1)" \
                --no-lthreads


Examples
~~~~~~~~

For selected scenarios the command line configuration of the application for L-threads
and its corresponding EAL threads command line can be realized as follows:

a) Start every thread on different scheduler (1:1)::

      dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                   --rx="(0,0,0,0)(1,0,1,1)" \
                   --tx="(2,0)(3,1)"

   EAL thread equivalent::

      dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                   --rx="(0,0,0,0)(1,0,1,1)" \
                   --tx="(2,0)(3,1)" \
                   --no-lthreads

b) Start all threads on one core (N:1).

   Start 4 L-threads on lcore 0::

      dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                   --rx="(0,0,0,0)(1,0,0,1)" \
                   --tx="(0,0)(0,1)"

   Start 4 EAL threads on cpu-set 0::

      dpdk-l3fwd-thread -l 0-7 -n 2 --lcores="(0-3)@0" -- -P -p 3 \
                   --rx="(0,0,0,0)(1,0,0,1)" \
                   --tx="(2,0)(3,1)" \
                   --no-lthreads

c) Start threads on different cores (N:M).

   Start 2 L-threads for RX on lcore 0, and 2 L-threads for TX on lcore 1::

      dpdk-l3fwd-thread -l 0-7 -n 2 -- -P -p 3 \
                   --rx="(0,0,0,0)(1,0,0,1)" \
                   --tx="(1,0)(1,1)"

   Start 2 EAL threads for RX on cpu-set 0, and 2 EAL threads for TX on
   cpu-set 1::

      dpdk-l3fwd-thread -l 0-7 -n 2 --lcores="(0-1)@0,(2-3)@1" -- -P -p 3 \
                   --rx="(0,0,0,0)(1,0,1,1)" \
                   --tx="(2,0)(3,1)" \
                   --no-lthreads

Explanation
-----------

To a great extent the sample application differs little from the standard L3
forwarding application, and readers are advised to familiarize themselves with
the material covered in the :doc:`l3_forward` documentation before proceeding.

The following explanation is focused on the way threading is handled in the
performance thread example.


Mode of operation with EAL threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The performance thread sample application has split the RX and TX functionality
into two different threads, and the RX and TX threads are
interconnected via software rings. With respect to these rings the RX threads
are producers and the TX threads are consumers.

On initialization the TX and RX threads are started according to the command
line parameters.

The RX threads poll the network interface queues and post received packets to a
TX thread via a corresponding software ring.

The TX threads poll software rings, perform the L3 forwarding hash/LPM match,
and assemble packet bursts before performing burst transmit on the network
interface.

As with the standard L3 forward application, burst draining of residual packets
is performed periodically with the period calculated from elapsed time using
the timestamps counter.

The diagram below illustrates a case with two RX threads and three TX threads.

.. _figure_performance_thread_1:

.. figure:: img/performance_thread_1.*


Mode of operation with L-threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Like the EAL thread configuration the application has split the RX and TX
functionality into different threads, and the pairs of RX and TX threads are
interconnected via software rings.

On initialization an L-thread scheduler is started on every EAL thread. On all
but the main EAL thread only a dummy L-thread is initially started.
The L-thread started on the main EAL thread then spawns other L-threads on
different L-thread schedulers according the command line parameters.

The RX threads poll the network interface queues and post received packets
to a TX thread via the corresponding software ring.

The ring interface is augmented by means of an L-thread condition variable that
enables the TX thread to be suspended when the TX ring is empty. The RX thread
signals the condition whenever it posts to the TX ring, causing the TX thread
to be resumed.

Additionally the TX L-thread spawns a worker L-thread to take care of
polling the software rings, whilst it handles burst draining of the transmit
buffer.

The worker threads poll the software rings, perform L3 route lookup and
assemble packet bursts. If the TX ring is empty the worker thread suspends
itself by waiting on the condition variable associated with the ring.

Burst draining of residual packets, less than the burst size, is performed by
the TX thread which sleeps (using an L-thread sleep function) and resumes
periodically to flush the TX buffer.

This design means that L-threads that have no work, can yield the CPU to other
L-threads and avoid having to constantly poll the software rings.

The diagram below illustrates a case with two RX threads and three TX functions
(each comprising a thread that processes forwarding and a thread that
periodically drains the output buffer of residual packets).

.. _figure_performance_thread_2:

.. figure:: img/performance_thread_2.*


CPU load statistics
~~~~~~~~~~~~~~~~~~~

It is possible to display statistics showing estimated CPU load on each core.
The statistics indicate the percentage of CPU time spent: processing
received packets (forwarding), polling queues/rings (waiting for work),
and doing any other processing (context switch and other overhead).

When enabled statistics are gathered by having the application threads set and
clear flags when they enter and exit pertinent code sections. The flags are
then sampled in real time by a statistics collector thread running on another
core. This thread displays the data in real time on the console.

This feature is enabled by designating a statistics collector core, using the
``--stat-lcore`` parameter.


.. _lthread_subsystem:

The L-thread subsystem
----------------------

The L-thread subsystem resides in the examples/performance-thread/common
directory and is built and linked automatically when building the
``l3fwd-thread`` example.

The subsystem provides a simple cooperative scheduler to enable arbitrary
functions to run as cooperative threads within a single EAL thread.
The subsystem provides a pthread like API that is intended to assist in
reuse of legacy code written for POSIX pthreads.

The following sections provide some detail on the features, constraints,
performance and porting considerations when using L-threads.


.. _comparison_between_lthreads_and_pthreads:

Comparison between L-threads and POSIX pthreads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The fundamental difference between the L-thread and pthread models is the
way in which threads are scheduled. The simplest way to think about this is to
consider the case of a processor with a single CPU. To run multiple threads
on a single CPU, the scheduler must frequently switch between the threads,
in order that each thread is able to make timely progress.
This is the basis of any multitasking operating system.

This section explores the differences between the pthread model and the
L-thread model as implemented in the provided L-thread subsystem. If needed a
theoretical discussion of preemptive vs cooperative multi-threading can be
found in any good text on operating system design.


Scheduling and context switching
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The POSIX pthread library provides an application programming interface to
create and synchronize threads. Scheduling policy is determined by the host OS,
and may be configurable. The OS may use sophisticated rules to determine which
thread should be run next, threads may suspend themselves or make other threads
ready, and the scheduler may employ a time slice giving each thread a maximum
time quantum after which it will be preempted in favor of another thread that
is ready to run. To complicate matters further threads may be assigned
different scheduling priorities.

By contrast the L-thread subsystem is considerably simpler. Logically the
L-thread scheduler performs the same multiplexing function for L-threads
within a single pthread as the OS scheduler does for pthreads within an
application process. The L-thread scheduler is simply the main loop of a
pthread, and in so far as the host OS is concerned it is a regular pthread
just like any other. The host OS is oblivious about the existence of and
not at all involved in the scheduling of L-threads.

The other and most significant difference between the two models is that
L-threads are scheduled cooperatively. L-threads cannot not preempt each
other, nor can the L-thread scheduler preempt a running L-thread (i.e.
there is no time slicing). The consequence is that programs implemented with
L-threads must possess frequent rescheduling points, meaning that they must
explicitly and of their own volition return to the scheduler at frequent
intervals, in order to allow other L-threads an opportunity to proceed.

In both models switching between threads requires that the current CPU
context is saved and a new context (belonging to the next thread ready to run)
is restored. With pthreads this context switching is handled transparently
and the set of CPU registers that must be preserved between context switches
is as per an interrupt handler.

An L-thread context switch is achieved by the thread itself making a function
call to the L-thread scheduler. Thus it is only necessary to preserve the
callee registers. The caller is responsible to save and restore any other
registers it is using before a function call, and restore them on return,
and this is handled by the compiler. For ``X86_64`` on both Linux and BSD the
System V calling convention is used, this defines registers RSP, RBP, and
R12-R15 as callee-save registers (for more detailed discussion a good reference
is `X86 Calling Conventions <https://en.wikipedia.org/wiki/X86_calling_conventions>`_).

Taking advantage of this, and due to the absence of preemption, an L-thread
context switch is achieved with less than 20 load/store instructions.

The scheduling policy for L-threads is fixed, there is no prioritization of
L-threads, all L-threads are equal and scheduling is based on a FIFO
ready queue.

An L-thread is a struct containing the CPU context of the thread
(saved on context switch) and other useful items. The ready queue contains
pointers to threads that are ready to run. The L-thread scheduler is a simple
loop that polls the ready queue, reads from it the next thread ready to run,
which it resumes by saving the current context (the current position in the
scheduler loop) and restoring the context of the next thread from its thread
struct. Thus an L-thread is always resumed at the last place it yielded.

A well behaved L-thread will call the context switch regularly (at least once
in its main loop) thus returning to the scheduler's own main loop. Yielding
inserts the current thread at the back of the ready queue, and the process of
servicing the ready queue is repeated, thus the system runs by flipping back
and forth the between L-threads and scheduler loop.

In the case of pthreads, the preemptive scheduling, time slicing, and support
for thread prioritization means that progress is normally possible for any
thread that is ready to run. This comes at the price of a relatively heavier
context switch and scheduling overhead.

With L-threads the progress of any particular thread is determined by the
frequency of rescheduling opportunities in the other L-threads. This means that
an errant L-thread monopolizing the CPU might cause scheduling of other threads
to be stalled. Due to the lower cost of context switching, however, voluntary
rescheduling to ensure progress of other threads, if managed sensibly, is not
a prohibitive overhead, and overall performance can exceed that of an
application using pthreads.


Mutual exclusion
^^^^^^^^^^^^^^^^

With pthreads preemption means that threads that share data must observe
some form of mutual exclusion protocol.

The fact that L-threads cannot preempt each other means that in many cases
mutual exclusion devices can be completely avoided.

Locking to protect shared data can be a significant bottleneck in
multi-threaded applications so a carefully designed cooperatively scheduled
program can enjoy significant performance advantages.

So far we have considered only the simplistic case of a single core CPU,
when multiple CPUs are considered things are somewhat more complex.

First of all it is inevitable that there must be multiple L-thread schedulers,
one running on each EAL thread. So long as these schedulers remain isolated
from each other the above assertions about the potential advantages of
cooperative scheduling hold true.

A configuration with isolated cooperative schedulers is less flexible than the
pthread model where threads can be affinitized to run on any CPU. With isolated
schedulers scaling of applications to utilize fewer or more CPUs according to
system demand is very difficult to achieve.

The L-thread subsystem makes it possible for L-threads to migrate between
schedulers running on different CPUs. Needless to say if the migration means
that threads that share data end up running on different CPUs then this will
introduce the need for some kind of mutual exclusion system.

Of course ``rte_ring`` software rings can always be used to interconnect
threads running on different cores, however to protect other kinds of shared
data structures, lock free constructs or else explicit locking will be
required. This is a consideration for the application design.

In support of this extended functionality, the L-thread subsystem implements
thread safe mutexes and condition variables.

The cost of affinitizing and of condition variable signaling is significantly
lower than the equivalent pthread operations, and so applications using these
features will see a performance benefit.


Thread local storage
^^^^^^^^^^^^^^^^^^^^

As with applications written for pthreads an application written for L-threads
can take advantage of thread local storage, in this case local to an L-thread.
An application may save and retrieve a single pointer to application data in
the L-thread struct.

For legacy and backward compatibility reasons two alternative methods are also
offered, the first is modeled directly on the pthread get/set specific APIs,
the second approach is modeled on the ``RTE_PER_LCORE`` macros, whereby
``PER_LTHREAD`` macros are introduced, in both cases the storage is local to
the L-thread.


.. _constraints_and_performance_implications:

Constraints and performance implications when using L-threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. _API_compatibility:

API compatibility
^^^^^^^^^^^^^^^^^

The L-thread subsystem provides a set of functions that are logically equivalent
to the corresponding functions offered by the POSIX pthread library, however not
all pthread functions have a corresponding L-thread equivalent, and not all
features available to pthreads are implemented for L-threads.

The pthread library offers considerable flexibility via programmable attributes
that can be associated with threads, mutexes, and condition variables.

By contrast the L-thread subsystem has fixed functionality, the scheduler policy
cannot be varied, and L-threads cannot be prioritized. There are no variable
attributes associated with any L-thread objects. L-threads, mutexes and
conditional variables, all have fixed functionality. (Note: reserved parameters
are included in the APIs to facilitate possible future support for attributes).

The table below lists the pthread and equivalent L-thread APIs with notes on
differences and/or constraints. Where there is no L-thread entry in the table,
then the L-thread subsystem provides no equivalent function.

.. _table_lthread_pthread:

.. table:: Pthread and equivalent L-thread APIs.

   +----------------------------+------------------------+-------------------+
   | **Pthread function**       | **L-thread function**  | **Notes**         |
   +============================+========================+===================+
   | pthread_barrier_destroy    |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_barrier_init       |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_barrier_wait       |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_cond_broadcast     | lthread_cond_broadcast | See note 1        |
   +----------------------------+------------------------+-------------------+
   | pthread_cond_destroy       | lthread_cond_destroy   |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_cond_init          | lthread_cond_init      |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_cond_signal        | lthread_cond_signal    | See note 1        |
   +----------------------------+------------------------+-------------------+
   | pthread_cond_timedwait     |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_cond_wait          | lthread_cond_wait      | See note 5        |
   +----------------------------+------------------------+-------------------+
   | pthread_create             | lthread_create         | See notes 2, 3    |
   +----------------------------+------------------------+-------------------+
   | pthread_detach             | lthread_detach         | See note 4        |
   +----------------------------+------------------------+-------------------+
   | pthread_equal              |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_exit               | lthread_exit           |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_getspecific        | lthread_getspecific    |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_getcpuclockid      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_join               | lthread_join           |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_key_create         | lthread_key_create     |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_key_delete         | lthread_key_delete     |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_mutex_destroy      | lthread_mutex_destroy  |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_mutex_init         | lthread_mutex_init     |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_mutex_lock         | lthread_mutex_lock     | See note 6        |
   +----------------------------+------------------------+-------------------+
   | pthread_mutex_trylock      | lthread_mutex_trylock  | See note 6        |
   +----------------------------+------------------------+-------------------+
   | pthread_mutex_timedlock    |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_mutex_unlock       | lthread_mutex_unlock   |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_once               |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_destroy     |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_init        |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_rdlock      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_timedrdlock |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_timedwrlock |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_tryrdlock   |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_trywrlock   |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_unlock      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_rwlock_wrlock      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_self               | lthread_current        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_setspecific        | lthread_setspecific    |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_spin_init          |                        | See note 10       |
   +----------------------------+------------------------+-------------------+
   | pthread_spin_destroy       |                        | See note 10       |
   +----------------------------+------------------------+-------------------+
   | pthread_spin_lock          |                        | See note 10       |
   +----------------------------+------------------------+-------------------+
   | pthread_spin_trylock       |                        | See note 10       |
   +----------------------------+------------------------+-------------------+
   | pthread_spin_unlock        |                        | See note 10       |
   +----------------------------+------------------------+-------------------+
   | pthread_cancel             | lthread_cancel         |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_setcancelstate     |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_setcanceltype      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_testcancel         |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_getschedparam      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_setschedparam      |                        |                   |
   +----------------------------+------------------------+-------------------+
   | pthread_yield              | lthread_yield          | See note 7        |
   +----------------------------+------------------------+-------------------+
   | pthread_setaffinity_np     | lthread_set_affinity   | See notes 2, 3, 8 |
   +----------------------------+------------------------+-------------------+
   |                            | lthread_sleep          | See note 9        |
   +----------------------------+------------------------+-------------------+
   |                            | lthread_sleep_clks     | See note 9        |
   +----------------------------+------------------------+-------------------+


**Note 1**:

Neither lthread signal nor broadcast may be called concurrently by L-threads
running on different schedulers, although multiple L-threads running in the
same scheduler may freely perform signal or broadcast operations. L-threads
running on the same or different schedulers may always safely wait on a
condition variable.


**Note 2**:

Pthread attributes may be used to affinitize a pthread with a cpu-set. The
L-thread subsystem does not support a cpu-set. An L-thread may be affinitized
only with a single CPU at any time.


**Note 3**:

If an L-thread is intended to run on a different NUMA node than the node that
creates the thread then, when calling ``lthread_create()`` it is advantageous
to specify the destination core as a parameter of ``lthread_create()``. See
:ref:`memory_allocation_and_NUMA_awareness` for details.


**Note 4**:

An L-thread can only detach itself, and cannot detach other L-threads.


**Note 5**:

A wait operation on a pthread condition variable is always associated with and
protected by a mutex which must be owned by the thread at the time it invokes
``pthread_wait()``. By contrast L-thread condition variables are thread safe
(for waiters) and do not use an associated mutex. Multiple L-threads (including
L-threads running on other schedulers) can safely wait on a L-thread condition
variable. As a consequence the performance of an L-thread condition variables
is typically an order of magnitude faster than its pthread counterpart.


**Note 6**:

Recursive locking is not supported with L-threads, attempts to take a lock
recursively will be detected and rejected.


**Note 7**:

``lthread_yield()`` will save the current context, insert the current thread
to the back of the ready queue, and resume the next ready thread. Yielding
increases ready queue backlog, see :ref:`ready_queue_backlog` for more details
about the implications of this.


N.B. The context switch time as measured from immediately before the call to
``lthread_yield()`` to the point at which the next ready thread is resumed,
can be an order of magnitude faster that the same measurement for
pthread_yield.


**Note 8**:

``lthread_set_affinity()`` is similar to a yield apart from the fact that the
yielding thread is inserted into a peer ready queue of another scheduler.
The peer ready queue is actually a separate thread safe queue, which means that
threads appearing in the peer ready queue can jump any backlog in the local
ready queue on the destination scheduler.

The context switch time as measured from the time just before the call to
``lthread_set_affinity()`` to just after the same thread is resumed on the new
scheduler can be orders of magnitude faster than the same measurement for
``pthread_setaffinity_np()``.


**Note 9**:

Although there is no ``pthread_sleep()`` function, ``lthread_sleep()`` and
``lthread_sleep_clks()`` can be used wherever ``sleep()``, ``usleep()`` or
``nanosleep()`` might ordinarily be used. The L-thread sleep functions suspend
the current thread, start an ``rte_timer`` and resume the thread when the
timer matures. The ``rte_timer_manage()`` entry point is called on every pass
of the scheduler loop. This means that the worst case jitter on timer expiry
is determined by the longest period between context switches of any running
L-threads.

In a synthetic test with many threads sleeping and resuming then the measured
jitter is typically orders of magnitude lower than the same measurement made
for ``nanosleep()``.


**Note 10**:

Spin locks are not provided because they are problematical in a cooperative
environment, see :ref:`porting_locks_and_spinlocks` for a more detailed
discussion on how to avoid spin locks.


.. _Thread_local_storage_performance:

Thread local storage
^^^^^^^^^^^^^^^^^^^^

Of the three L-thread local storage options the simplest and most efficient is
storing a single application data pointer in the L-thread struct.

The ``PER_LTHREAD`` macros involve a run time computation to obtain the address
of the variable being saved/retrieved and also require that the accesses are
de-referenced  via a pointer. This means that code that has used
``RTE_PER_LCORE`` macros being ported to L-threads might need some slight
adjustment (see :ref:`porting_thread_local_storage` for hints about porting
code that makes use of thread local storage).

The get/set specific APIs are consistent with their pthread counterparts both
in use and in performance.


.. _memory_allocation_and_NUMA_awareness:

Memory allocation and NUMA awareness
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

All memory allocation is from DPDK huge pages, and is NUMA aware. Each
scheduler maintains its own caches of objects: lthreads, their stacks, TLS,
mutexes and condition variables. These caches are implemented as unbounded lock
free MPSC queues. When objects are created they are always allocated from the
caches on the local core (current EAL thread).

If an L-thread has been affinitized to a different scheduler, then it can
always safely free resources to the caches from which they originated (because
the caches are MPSC queues).

If the L-thread has been affinitized to a different NUMA node then the memory
resources associated with it may incur longer access latency.

The commonly used pattern of setting affinity on entry to a thread after it has
started, means that memory allocation for both the stack and TLS will have been
made from caches on the NUMA node on which the threads creator is running.
This has the side effect that access latency will be sub-optimal after
affinitizing.

This side effect can be mitigated to some extent (although not completely) by
specifying the destination CPU as a parameter of ``lthread_create()`` this
causes the L-thread's stack and TLS to be allocated when it is first scheduled
on the destination scheduler, if the destination is a on another NUMA node it
results in a more optimal memory allocation.

Note that the lthread struct itself remains allocated from memory on the
creating node, this is unavoidable because an L-thread is known everywhere by
the address of this struct.


.. _object_cache_sizing:

Object cache sizing
^^^^^^^^^^^^^^^^^^^

The per lcore object caches pre-allocate objects in bulk whenever a request to
allocate an object finds a cache empty. By default 100 objects are
pre-allocated, this is defined by ``LTHREAD_PREALLOC`` in the public API
header file lthread_api.h. This means that the caches constantly grow to meet
system demand.

In the present implementation there is no mechanism to reduce the cache sizes
if system demand reduces. Thus the caches will remain at their maximum extent
indefinitely.

A consequence of the bulk pre-allocation of objects is that every 100 (default
value) additional new object create operations results in a call to
``rte_malloc()``. For creation of objects such as L-threads, which trigger the
allocation of even more objects (i.e. their stacks and TLS) then this can
cause outliers in scheduling performance.

If this is a problem the simplest mitigation strategy is to dimension the
system, by setting the bulk object pre-allocation size to some large number
that you do not expect to be exceeded. This means the caches will be populated
once only, the very first time a thread is created.


.. _Ready_queue_backlog:

Ready queue backlog
^^^^^^^^^^^^^^^^^^^

One of the more subtle performance considerations is managing the ready queue
backlog. The fewer threads that are waiting in the ready queue then the faster
any particular thread will get serviced.

In a naive L-thread application with N L-threads simply looping and yielding,
this backlog will always be equal to the number of L-threads, thus the cost of
a yield to a particular L-thread will be N times the context switch time.

This side effect can be mitigated by arranging for threads to be suspended and
wait to be resumed, rather than polling for work by constantly yielding.
Blocking on a mutex or condition variable or even more obviously having a
thread sleep if it has a low frequency workload are all mechanisms by which a
thread can be excluded from the ready queue until it really does need to be
run. This can have a significant positive impact on performance.


.. _Initialization_and_shutdown_dependencies:

Initialization, shutdown and dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The L-thread subsystem depends on DPDK for huge page allocation and depends on
the ``rte_timer subsystem``. The DPDK EAL initialization and
``rte_timer_subsystem_init()`` **MUST** be completed before the L-thread sub
system can be used.

Thereafter initialization of the L-thread subsystem is largely transparent to
the application. Constructor functions ensure that global variables are properly
initialized. Other than global variables each scheduler is initialized
independently the first time that an L-thread is created by a particular EAL
thread.

If the schedulers are to be run as isolated and independent schedulers, with
no intention that L-threads running on different schedulers will migrate between
schedulers or synchronize with L-threads running on other schedulers, then
initialization consists simply of creating an L-thread, and then running the
L-thread scheduler.

If there will be interaction between L-threads running on different schedulers,
then it is important that the starting of schedulers on different EAL threads
is synchronized.

To achieve this an additional initialization step is necessary, this is simply
to set the number of schedulers by calling the API function
``lthread_num_schedulers_set(n)``, where ``n`` is the number of EAL threads
that will run L-thread schedulers. Setting the number of schedulers to a
number greater than 0 will cause all schedulers to wait until the others have
started before beginning to schedule L-threads.

The L-thread scheduler is started by calling the function ``lthread_run()``
and should be called from the EAL thread and thus become the main loop of the
EAL thread.

The function ``lthread_run()``, will not return until all threads running on
the scheduler have exited, and the scheduler has been explicitly stopped by
calling ``lthread_scheduler_shutdown(lcore)`` or
``lthread_scheduler_shutdown_all()``.

All these function do is tell the scheduler that it can exit when there are no
longer any running L-threads, neither function forces any running L-thread to
terminate. Any desired application shutdown behavior must be designed and
built into the application to ensure that L-threads complete in a timely
manner.

**Important Note:** It is assumed when the scheduler exits that the application
is terminating for good, the scheduler does not free resources before exiting
and running the scheduler a subsequent time will result in undefined behavior.


.. _porting_legacy_code_to_run_on_lthreads:

Porting legacy code to run on L-threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Legacy code originally written for a pthread environment may be ported to
L-threads if the considerations about differences in scheduling policy, and
constraints discussed in the previous sections can be accommodated.

This section looks in more detail at some of the issues that may have to be
resolved when porting code.


.. _pthread_API_compatibility:

pthread API compatibility
^^^^^^^^^^^^^^^^^^^^^^^^^

The first step is to establish exactly which pthread APIs the legacy
application uses, and to understand the requirements of those APIs. If there
are corresponding L-lthread APIs, and where the default pthread functionality
is used by the application then, notwithstanding the other issues discussed
here, it should be feasible to run the application with L-threads. If the
legacy code modifies the default behavior using attributes then if may be
necessary to make some adjustments to eliminate those requirements.


.. _blocking_system_calls:

Blocking system API calls
^^^^^^^^^^^^^^^^^^^^^^^^^

It is important to understand what other system services the application may be
using, bearing in mind that in a cooperatively scheduled environment a thread
cannot block without stalling the scheduler and with it all other cooperative
threads. Any kind of blocking system call, for example file or socket IO, is a
potential problem, a good tool to analyze the application for this purpose is
the ``strace`` utility.

There are many strategies to resolve these kind of issues, each with it
merits. Possible solutions include:

* Adopting a polled mode of the system API concerned (if available).

* Arranging for another core to perform the function and synchronizing with
  that core via constructs that will not block the L-thread.

* Affinitizing the thread to another scheduler devoted (as a matter of policy)
  to handling threads wishing to make blocking calls, and then back again when
  finished.


.. _porting_locks_and_spinlocks:

Locks and spinlocks
^^^^^^^^^^^^^^^^^^^

Locks and spinlocks are another source of blocking behavior that for the same
reasons as system calls will need to be addressed.

If the application design ensures that the contending L-threads will always
run on the same scheduler then it its probably safe to remove locks and spin
locks completely.

The only exception to the above rule is if for some reason the
code performs any kind of context switch whilst holding the lock
(e.g. yield, sleep, or block on a different lock, or on a condition variable).
This will need to determined before deciding to eliminate a lock.

If a lock cannot be eliminated then an L-thread mutex can be substituted for
either kind of lock.

An L-thread blocking on an L-thread mutex will be suspended and will cause
another ready L-thread to be resumed, thus not blocking the scheduler. When
default behavior is required, it can be used as a direct replacement for a
pthread mutex lock.

Spin locks are typically used when lock contention is likely to be rare and
where the period during which the lock may be held is relatively short.
When the contending L-threads are running on the same scheduler then an
L-thread blocking on a spin lock will enter an infinite loop stopping the
scheduler completely (see :ref:`porting_infinite_loops` below).

If the application design ensures that contending L-threads will always run
on different schedulers then it might be reasonable to leave a short spin lock
that rarely experiences contention in place.

If after all considerations it appears that a spin lock can neither be
eliminated completely, replaced with an L-thread mutex, or left in place as
is, then an alternative is to loop on a flag, with a call to
``lthread_yield()`` inside the loop (n.b. if the contending L-threads might
ever run on different schedulers the flag will need to be manipulated
atomically).

Spinning and yielding is the least preferred solution since it introduces
ready queue backlog (see also :ref:`ready_queue_backlog`).


.. _porting_sleeps_and_delays:

Sleeps and delays
^^^^^^^^^^^^^^^^^

Yet another kind of blocking behavior (albeit momentary) are delay functions
like ``sleep()``, ``usleep()``, ``nanosleep()`` etc. All will have the
consequence of stalling the L-thread scheduler and unless the delay is very
short (e.g. a very short nanosleep) calls to these functions will need to be
eliminated.

The simplest mitigation strategy is to use the L-thread sleep API functions,
of which two variants exist, ``lthread_sleep()`` and ``lthread_sleep_clks()``.
These functions start an rte_timer against the L-thread, suspend the L-thread
and cause another ready L-thread to be resumed. The suspended L-thread is
resumed when the rte_timer matures.


.. _porting_infinite_loops:

Infinite loops
^^^^^^^^^^^^^^

Some applications have threads with loops that contain no inherent
rescheduling opportunity, and rely solely on the OS time slicing to share
the CPU. In a cooperative environment this will stop everything dead. These
kind of loops are not hard to identify, in a debug session you will find the
debugger is always stopping in the same loop.

The simplest solution to this kind of problem is to insert an explicit
``lthread_yield()`` or ``lthread_sleep()`` into the loop. Another solution
might be to include the function performed by the loop into the execution path
of some other loop that does in fact yield, if this is possible.


.. _porting_thread_local_storage:

Thread local storage
^^^^^^^^^^^^^^^^^^^^

If the application uses thread local storage, the use case should be
studied carefully.

In a legacy pthread application either or both the ``__thread`` prefix, or the
pthread set/get specific APIs may have been used to define storage local to a
pthread.

In some applications it may be a reasonable assumption that the data could
or in fact most likely should be placed in L-thread local storage.

If the application (like many DPDK applications) has assumed a certain
relationship between a pthread and the CPU to which it is affinitized, there
is a risk that thread local storage may have been used to save some data items
that are correctly logically associated with the CPU, and others items which
relate to application context for the thread. Only a good understanding of the
application will reveal such cases.

If the application requires an that an L-thread is to be able to move between
schedulers then care should be taken to separate these kinds of data, into per
lcore, and per L-thread storage. In this way a migrating thread will bring with
it the local data it needs, and pick up the new logical core specific values
from pthread local storage at its new home.


.. _pthread_shim:

Pthread shim
~~~~~~~~~~~~

A convenient way to get something working with legacy code can be to use a
shim that adapts pthread API calls to the corresponding L-thread ones.
This approach will not mitigate any of the porting considerations mentioned
in the previous sections, but it will reduce the amount of code churn that
would otherwise been involved. It is a reasonable approach to evaluate
L-threads, before investing effort in porting to the native L-thread APIs.


Overview
^^^^^^^^
The L-thread subsystem includes an example pthread shim. This is a partial
implementation but does contain the API stubs needed to get basic applications
running. There is a simple "hello world" application that demonstrates the
use of the pthread shim.

A subtlety of working with a shim is that the application will still need
to make use of the genuine pthread library functions, at the very least in
order to create the EAL threads in which the L-thread schedulers will run.
This is the case with DPDK initialization, and exit.

To deal with the initialization and shutdown scenarios, the shim is capable of
switching on or off its adaptor functionality, an application can control this
behavior by the calling the function ``pt_override_set()``. The default state
is disabled.

The pthread shim uses the dynamic linker loader and saves the loaded addresses
of the genuine pthread API functions in an internal table, when the shim
functionality is enabled it performs the adaptor function, when disabled it
invokes the genuine pthread function.

The function ``pthread_exit()`` has additional special handling. The standard
system header file pthread.h declares ``pthread_exit()`` with
``__rte_noreturn`` this is an optimization that is possible because
the pthread is terminating and this enables the compiler to omit the normal
handling of stack and protection of registers since the function is not
expected to return, and in fact the thread is being destroyed. These
optimizations are applied in both the callee and the caller of the
``pthread_exit()`` function.

In our cooperative scheduling environment this behavior is inadmissible. The
pthread is the L-thread scheduler thread, and, although an L-thread is
terminating, there must be a return to the scheduler in order that the system
can continue to run. Further, returning from a function with attribute
``noreturn`` is invalid and may result in undefined behavior.

The solution is to redefine the ``pthread_exit`` function with a macro,
causing it to be mapped to a stub function in the shim that does not have the
``noreturn`` attribute. This macro is defined in the file
``pthread_shim.h``. The stub function is otherwise no different than any of
the other stub functions in the shim, and will switch between the real
``pthread_exit()`` function or the ``lthread_exit()`` function as
required. The only difference is that the mapping to the stub by macro
substitution.

A consequence of this is that the file ``pthread_shim.h`` must be included in
legacy code wishing to make use of the shim. It also means that dynamic
linkage of a pre-compiled binary that did not include pthread_shim.h is not be
supported.

Given the requirements for porting legacy code outlined in
:ref:`porting_legacy_code_to_run_on_lthreads` most applications will require at
least some minimal adjustment and recompilation to run on L-threads so
pre-compiled binaries are unlikely to be met in practice.

In summary the shim approach adds some overhead but can be a useful tool to help
establish the feasibility of a code reuse project. It is also a fairly
straightforward task to extend the shim if necessary.

**Note:** Bearing in mind the preceding discussions about the impact of making
blocking calls then switching the shim in and out on the fly to invoke any
pthread API this might block is something that should typically be avoided.


Building and running the pthread shim
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The shim example application is located in the sample application
in the performance-thread folder

To build and run the pthread shim example

#. Build the application:

   To compile the sample application see :doc:`compiling`.

#. To run the pthread_shim example

   .. code-block:: console

       dpdk-pthread-shim -c core_mask -n number_of_channels

.. _lthread_diagnostics:

L-thread Diagnostics
~~~~~~~~~~~~~~~~~~~~

When debugging you must take account of the fact that the L-threads are run in
a single pthread. The current scheduler is defined by
``RTE_PER_LCORE(this_sched)``, and the current lthread is stored at
``RTE_PER_LCORE(this_sched)->current_lthread``. Thus on a breakpoint in a GDB
session the current lthread can be obtained by displaying the pthread local
variable ``per_lcore_this_sched->current_lthread``.

Another useful diagnostic feature is the possibility to trace significant
events in the life of an L-thread, this feature is enabled by changing the
value of LTHREAD_DIAG from 0 to 1 in the file ``lthread_diag_api.h``.

Tracing of events can be individually masked, and the mask may be programmed
at run time. An unmasked event results in a callback that provides information
about the event. The default callback simply prints trace information. The
default mask is 0 (all events off) the mask can be modified by calling the
function ``lthread_diagnostic_set_mask()``.

It is possible register a user callback function to implement more
sophisticated diagnostic functions.
Object creation events (lthread, mutex, and condition variable) accept, and
store in the created object, a user supplied reference value returned by the
callback function.

The lthread reference value is passed back in all subsequent event callbacks,
the mutex and APIs are provided to retrieve the reference value from
mutexes and condition variables. This enables a user to monitor, count, or
filter for specific events, on specific objects, for example to monitor for a
specific thread signaling a specific condition variable, or to monitor
on all timer events, the possibilities and combinations are endless.

The callback function can be set by calling the function
``lthread_diagnostic_enable()`` supplying a callback function pointer and an
event mask.

Setting ``LTHREAD_DIAG`` also enables counting of statistics about cache and
queue usage, and these statistics can be displayed by calling the function
``lthread_diag_stats_display()``. This function also performs a consistency
check on the caches and queues. The function should only be called from the
main EAL thread after all worker threads have stopped and returned to the C
main program, otherwise the consistency check will fail.
