..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Timer_Library:

Timer Library
=============

The Timer library provides a timer service to DPDK execution units to enable execution of callback functions asynchronously.
Features of the library are:

*   Timers can be periodic (multi-shot) or single (one-shot).

*   Timers can be loaded from one core and executed on another. It has to be specified in the call to rte_timer_reset().

*   Timers provide high precision (depends on the call frequency to rte_timer_manage() that checks timer expiration for the local core).

*   If not required in the application, timers can be disabled at compilation time by not calling the rte_timer_manage() to increase performance.

The timer library uses the rte_get_timer_cycles() function that uses the High Precision Event Timer (HPET)
or the CPUs Time Stamp Counter (TSC) to provide a reliable time reference.

This library provides an interface to add, delete and restart a timer. The API is based on BSD callout() with a few differences.
Refer to the `callout manual <http://www.daemon-systems.org/man/callout.9.html>`_.

Implementation Details
----------------------

Timers are tracked on a per-lcore basis,
with all pending timers for a core being maintained in order of timer expiry in a skiplist data structure.
The skiplist used has ten levels and each entry in the table appears in each level with probability ¼^level.
This means that all entries are present in level 0, 1 in every 4 entries is present at level 1,
one in every 16 at level 2 and so on up to level 9.
This means that adding and removing entries from the timer list for a core can be done in log(n) time,
up to 4^10 entries, that is, approximately 1,000,000 timers per lcore.

A timer structure contains a special field called status,
which is a union of a timer state (stopped, pending, running, config) and an owner (lcore id).
Depending on the timer state, we know if a timer is present in a list or not:

*   STOPPED: no owner, not in a list

*   CONFIG: owned by a core, must not be modified by another core, maybe in a list or not, depending on previous state

*   PENDING: owned by a core, present in a list

*   RUNNING: owned by a core, must not be modified by another core, present in a list

Resetting or stopping a timer while it is in a CONFIG or RUNNING state is not allowed.
When modifying the state of a timer,
a Compare And Swap instruction should be used to guarantee that the status (state+owner) is modified atomically.

Inside the rte_timer_manage() function,
the skiplist is used as a regular list by iterating along the level 0 list, which contains all timer entries,
until an entry which has not yet expired has been encountered.
To improve performance in the case where there are entries in the timer list but none of those timers have yet expired,
the expiry time of the first list entry is maintained within the per-core timer list structure itself.
On 64-bit platforms, this value can be checked without the need to take a lock on the overall structure.
(Since expiry times are maintained as 64-bit values,
a check on the value cannot be done on 32-bit platforms without using either a compare-and-swap (CAS) instruction or using a lock,
so this additional check is skipped in favor of checking as normal once the lock has been taken.)
On both 64-bit and 32-bit platforms,
a call to rte_timer_manage() returns without taking a lock in the case where the timer list for the calling core is empty.

Use Cases
---------

The timer library is used for periodic calls, such as garbage collectors, or some state machines (ARP, bridging, and so on).

References
----------

*   `callout manual <http://www.daemon-systems.org/man/callout.9.html>`_
    - The callout facility that provides timers with a mechanism to execute a function at a given time.

*   `HPET <http://en.wikipedia.org/wiki/HPET>`_
    - Information about the High Precision Event Timer (HPET).
