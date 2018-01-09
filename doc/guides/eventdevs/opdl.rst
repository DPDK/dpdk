..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.

OPDL Eventdev Poll Mode Driver
==================================

The OPDL (Ordered Packet Distribution Library) eventdev is a specific\
implementation of the eventdev API. It is particularly suited to packet\
processing workloads that have high throughput and low latency requirements.\
All packets follow the same path through the device. The order in which\
packets  follow is determinted by the order in which queues are set up.\
Events are left on the ring until they are transmitted. As a result packets\
do not go out of order


Features
--------

The OPDL  eventdev implements a subset of features of the eventdev API;

Queues
 * Atomic
 * Ordered (Parallel is supported as parallel is a subset of Ordered)
 * Single-Link

Ports
 * Load balanced (for Atomic, Ordered, Parallel queues)
 * Single Link (for single-link queues)


Configuration and Options
-------------------------

The software eventdev is a vdev device, and as such can be created from the
application code, or from the EAL command line:

* Call ``rte_vdev_init("event_opdl0")`` from the application

* Use ``--vdev="event_opdl0"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_eventdev_application --vdev="event_opdl0"


Single Port Queue
~~~~~~~~~~~~~~~~~

It is possible to create a Single Port Queue ``RTE_EVENT_QUEUE_CFG_SINGLE_LINK``.
Packets dequeued from this queue do not need to be re-enqueued (as is the
case with an ordered queue). The purpose of this queue is to allow for
asynchronous handling of packets in the middle of a pipeline. Ordered
queues in the middle of a pipeline cannot delete packets.


Queue Dependencies
~~~~~~~~~~~~~~~~~~

As stated the order in which packets travel through queues is static in
nature. They go through the queues in the order the queues are setup at
initialisation ``rte_event_queue_setup()``. For example if an application
sets up 3 queues, Q0, Q1, Q2 and has 3 assoicated ports P0, P1, P2 and
P3 then packets must be

 * Enqueued onto Q0 (typically through P0), then

 * Dequeued from Q0 (typically through P1), then

 * Enqueued onto Q1 (also through P1), then

 * Dequeued from Q2 (typically through P2),  then

 * Enqueued onto Q3 (also through P2), then

 * Dequeued from Q3 (typically through P3) and then transmitted on the relevant \
   eth port


Limitations
-----------

The opdl implementation has a number of limitations. These limitations are
due to the static nature of the underlying queues. It is because of this
that the implementation can achieve such high throughput and low latency

The following list is a comprehensive outline of the what is supported and
the limitations / restrictions imposed by the opdl pmd

 - The order in which packets moved between queues is static and fixed \
   (dynamic scheduling is not supported).

 - NEW, RELEASE are not explicitly supported. RX (first enqueue) implicitly \
   adds NEW event types, and TX (last dequeue) implicitly does RELEASE event types.

 - All packets follow the same path through device queues.

 - Flows within queues are NOT supported.

 - Event priority is NOT supported.

 - Once the device is stopped all inflight events are lost. Applications should \
   clear all inflight events before stopping it.

 - Each port can only be associated with one queue.

 - Each queue can have multiple ports associated with it.

 - Each worker core has to dequeue the maximum burst size for that port.

 - For performance, the rte_event flow_id should not be updated once packet\
   is enqueued on RX.



Validation & Statistics
~~~~~~~~~~~~~~~~~~~~~~~

Validation can be turned on through a command line parameter

.. code-block:: console

    --vdev="event_opdl0,do_validation=1,self_test=1"

If validation is turned on every packet (as opposed to just the first in
each burst), is validated to have come from the right queue. Statistics
are also produced in this mode. The statistics are available through the
eventdev xstats API. Statistics are per port as follows:

 - claim_pkts_requested
 - claim_pkts_granted
 - claim_non_empty
 - claim_empty
 - total_cycles
