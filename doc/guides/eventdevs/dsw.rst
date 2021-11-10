..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Ericsson AB

Distributed Software Eventdev Poll Mode Driver
==============================================

The distributed software event device is an eventdev driver which
distributes the task of scheduling events among all the eventdev ports
and the lcore threads using them.

Features
--------

Queues
 * Atomic
 * Parallel
 * Single-Link

Ports
 * Load balanced (for Atomic, Ordered, Parallel queues)
 * Single Link (for single-link queues)

Configuration and Options
-------------------------

The distributed software eventdev is a vdev device, and as such can be
created from the application code, or from the EAL command line:

* Call ``rte_vdev_init("event_dsw0")`` from the application

* Use ``--vdev="event_dsw0"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_eventdev_application --vdev="event_dsw0"

Limitations
-----------

Port Maintenance
~~~~~~~~~~~~~~~~

The distributed software eventdev uses an internal signaling scheme
between the ports to achieve load balancing. Therefore, it does not
set the ``RTE_EVENT_DEV_CAP_MAINTENANCE_FREE`` flag.

During periods when the application thread using a particular port is
neither attempting to enqueue nor to dequeue events, it must
repeatedly call rte_event_maintain() on that port.

Ports left unmaintained for long periods of time will prevent load
balancing and cause traffic interruptions on flows which are in the
process of being migrated.

Output Buffering
~~~~~~~~~~~~~~~~

For efficiency reasons, the distributed software eventdev might not
send enqueued events immediately to the destination port, but instead
store them in an internal buffer in the source port.

In case no more events are enqueued on a port with buffered events,
these events will be sent after the application has performed a number
of enqueue and/or dequeue operations.

To immediately flush a port's output buffer, an application may call
rte_event_maintain() with op set to ``RTE_EVENT_DEV_MAINT_OP_FLUSH``.

Repeated calls to rte_event_maintain() will also flush the output
buffers.


Priorities
~~~~~~~~~~

The distributed software eventdev does not support event priorities.

Ordered Queues
~~~~~~~~~~~~~~

The distributed software eventdev does not support the ordered queue type.


"All Types" Queues
~~~~~~~~~~~~~~~~~~

The distributed software eventdev does not support queues of type
RTE_EVENT_QUEUE_CFG_ALL_TYPES, which allow both atomic, ordered, and
parallel events on the same queue.

Dynamic Link/Unlink
~~~~~~~~~~~~~~~~~~~

The distributed software eventdev does not support calls to
rte_event_port_link() or rte_event_port_unlink() after
rte_event_dev_start() has been called.
