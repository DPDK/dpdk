..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Software Eventdev Poll Mode Driver
==================================

The software eventdev is an implementation of the eventdev API, that provides a
wide range of the eventdev features. The eventdev relies on a CPU core to
perform event scheduling. This PMD can use the service core library to run the
scheduling function, allowing an application to utilize the power of service
cores to multiplex other work on the same core if required.


Features
--------

The software eventdev implements many features in the eventdev API;

Queues
 * Atomic
 * Ordered
 * Parallel
 * Single-Link

Ports
 * Load balanced (for Atomic, Ordered, Parallel queues)
 * Single Link (for single-link queues)

Event Priorities
 * Each event has a priority, which can be used to provide basic QoS


Configuration and Options
-------------------------

The software eventdev is a vdev device, and as such can be created from the
application code, or from the EAL command line:

* Call ``rte_vdev_init("event_sw0")`` from the application

* Use ``--vdev="event_sw0"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_eventdev_application --vdev="event_sw0"


Scheduling Quanta
~~~~~~~~~~~~~~~~~

The scheduling quanta sets the number of events that the device attempts to
schedule in a single schedule call performed by the service core. Note that
is a *hint* only, and that fewer or more events may be scheduled in a given
iteration.

The scheduling quanta can be set using a string argument to the vdev
create call:

.. code-block:: console

    --vdev="event_sw0,sched_quanta=64"


Credit Quanta
~~~~~~~~~~~~~

The credit quanta is the number of credits that a port will fetch at a time from
the instance's credit pool. Higher numbers will cause less overhead in the
atomic credit fetch code, however it also reduces the overall number of credits
in the system faster. A balanced number (eg 32) ensures that only small numbers
of credits are pre-allocated at a time, while also mitigating performance impact
of the atomics.

Experimentation with higher values may provide minor performance improvements,
at the cost of the whole system having less credits. On the other hand,
reducing the quanta may cause measurable performance impact but provide the
system with a higher number of credits at all times.

A value of 32 seems a good balance however your specific application may
benefit from a higher or reduced quanta size, experimentation is required to
verify possible gains.

.. code-block:: console

    --vdev="event_sw0,credit_quanta=64"


Limitations
-----------

The software eventdev implementation has a few limitations. The reason for
these limitations is usually that the performance impact of supporting the
feature would be significant.


"All Types" Queues
~~~~~~~~~~~~~~~~~~

The software eventdev does not support creating queues that handle all types of
traffic. An eventdev with this capability allows enqueueing Atomic, Ordered and
Parallel traffic to the same queue, but scheduling each of them appropriately.

The reason to not allow Atomic, Ordered and Parallel event types in the
same queue is that it causes excessive branching in the code to enqueue packets
to the queue, causing a significant performance impact.

The ``RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES`` flag is not set in the
``event_dev_cap`` field of the ``rte_event_dev_info`` struct for the software
eventdev.

Distributed Scheduler
~~~~~~~~~~~~~~~~~~~~~

The software eventdev is a centralized scheduler, requiring a service core to
perform the required event distribution. This is not really a limitation but
rather a design decision.

The ``RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED`` flag is not set in the
``event_dev_cap`` field of the ``rte_event_dev_info`` struct for the software
eventdev.

Dequeue Timeout
~~~~~~~~~~~~~~~

The eventdev API supports a timeout when dequeuing packets using the
``rte_event_dequeue_burst`` function.
This allows a core to wait for an event to arrive, or until ``timeout`` number
of ticks have passed. Timeout ticks is not supported by the software eventdev
for performance reasons.
