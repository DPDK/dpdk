..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.
    Copyright(c) 2018 Arm Limited.

Event Device Library
====================

The DPDK Event device library is an abstraction that provides the application
with features to schedule events. This is achieved using the PMD architecture
similar to the ethdev or cryptodev APIs, which may already be familiar to the
reader.

The eventdev framework introduces the event driven programming model. In a
polling model, lcores poll ethdev ports and associated Rx queues directly
to look for a packet. By contrast in an event driven model, lcores call the
scheduler that selects packets for them based on programmer-specified criteria.
The Eventdev library adds support for an event driven programming model, which
offers applications automatic multicore scaling, dynamic load balancing,
pipelining, packet ingress order maintenance and synchronization services to
simplify application packet processing.

By introducing an event driven programming model, DPDK can support both polling
and event driven programming models for packet processing, and applications are
free to choose whatever model (or combination of the two) best suits their
needs.

Step-by-step instructions of the eventdev design is available in the `API
Walk-through`_ section later in this document.

Event struct
------------

The eventdev API represents each event with a generic struct, which contains a
payload and metadata required for scheduling by an eventdev.  The
``rte_event`` struct is a 16 byte C structure, defined in
``libs/librte_eventdev/rte_eventdev.h``.

Event Metadata
~~~~~~~~~~~~~~

The rte_event structure contains the following metadata fields, which the
application fills in to have the event scheduled as required:

* ``flow_id`` - The targeted flow identifier for the enq/deq operation.
* ``event_type`` - The source of this event, e.g. RTE_EVENT_TYPE_ETHDEV or CPU.
* ``sub_event_type`` - Distinguishes events inside the application, that have
  the same event_type (see above)
* ``op`` - This field takes one of the RTE_EVENT_OP_* values, and tells the
  eventdev about the status of the event - valid values are NEW, FORWARD or
  RELEASE.
* ``sched_type`` - Represents the type of scheduling that should be performed
  on this event, valid values are the RTE_SCHED_TYPE_ORDERED, ATOMIC and
  PARALLEL.
* ``queue_id`` - The identifier for the event queue that the event is sent to.
* ``priority`` - The priority of this event, see RTE_EVENT_DEV_PRIORITY.

Event Payload
~~~~~~~~~~~~~

The rte_event struct contains a union for payload, allowing flexibility in what
the actual event being scheduled is. The payload is a union of the following:

* ``uint64_t u64``
* ``void *event_ptr``
* ``struct rte_mbuf *mbuf``
* ``struct rte_event_vector *vec``

These four items in a union occupy the same 64 bits at the end of the rte_event
structure. The application can utilize the 64 bits directly by accessing the
u64 variable, while the event_ptr, mbuf, vec are provided as a convenience
variables.  For example the mbuf pointer in the union can used to schedule a
DPDK packet.

Event Vector
~~~~~~~~~~~~

The rte_event_vector struct contains a vector of elements defined by the event
type specified in the ``rte_event``. The event_vector structure contains the
following data:

* ``nb_elem`` - The number of elements held within the vector.

Similar to ``rte_event`` the payload of event vector is also a union, allowing
flexibility in what the actual vector is.

* ``struct rte_mbuf *mbufs[0]`` - An array of mbufs.
* ``void *ptrs[0]`` - An array of pointers.
* ``uint64_t u64s[0]`` - An array of uint64_t elements.

The size of the event vector is related to the total number of elements it is
configured to hold, this is achieved by making `rte_event_vector` a variable
length structure.
A helper function is provided to create a mempool that holds event vector, which
takes name of the pool, total number of required ``rte_event_vector``,
cache size, number of elements in each ``rte_event_vector`` and socket id.

.. code-block:: c

        rte_event_vector_pool_create("vector_pool", nb_event_vectors, cache_sz,
                                     nb_elements_per_vector, socket_id);

The function ``rte_event_vector_pool_create`` creates mempool with the best
platform mempool ops.

Queues
~~~~~~

An event queue is a queue containing events that are scheduled by the event
device. An event queue contains events of different flows associated with
scheduling types, such as atomic, ordered, or parallel.

Queue All Types Capable
^^^^^^^^^^^^^^^^^^^^^^^

If RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES capability bit is set in the event device,
then events of any type may be sent to any queue. Otherwise, the queues only
support events of the type that it was created with.

Queue All Types Incapable
^^^^^^^^^^^^^^^^^^^^^^^^^

In this case, each stage has a specified scheduling type.  The application
configures each queue for a specific type of scheduling, and just enqueues all
events to the eventdev. An example of a PMD of this type is the eventdev
software PMD.

The Eventdev API supports the following scheduling types per queue:

*   Atomic
*   Ordered
*   Parallel

Atomic, Ordered and Parallel are load-balanced scheduling types: the output
of the queue can be spread out over multiple CPU cores.

Atomic scheduling on a queue ensures that a single flow is not present on two
different CPU cores at the same time. Ordered allows sending all flows to any
core, but the scheduler must ensure that on egress the packets are returned to
ingress order on downstream queue enqueue. Parallel allows sending all flows
to all CPU cores, without any re-ordering guarantees.

Single Link Flag
^^^^^^^^^^^^^^^^

There is a SINGLE_LINK flag which allows an application to indicate that only
one port will be connected to a queue.  Queues configured with the single-link
flag follow a FIFO like structure, maintaining ordering but it is only capable
of being linked to a single port (see below for port and queue linking details).


Ports
~~~~~

Ports are the points of contact between worker cores and the eventdev. The
general use case will see one CPU core using one port to enqueue and dequeue
events from an eventdev. Ports are linked to queues in order to retrieve events
from those queues (more details in `Linking Queues and Ports`_ below).


API Walk-through
----------------

This section will introduce the reader to the eventdev API, showing how to
create and configure an eventdev and use it for a two-stage atomic pipeline
with one core each for RX and TX. RX and TX cores are shown here for
illustration, refer to Eventdev Adapter documentation for further details.
The diagram below shows the final state of the application after this
walk-through:

.. _figure_eventdev-usage1:

.. figure:: ../img/eventdev_usage.*

   Sample eventdev usage, with RX, two atomic stages and a single-link to TX.


A high level overview of the setup steps are:

* rte_event_dev_configure()
* rte_event_queue_setup()
* rte_event_port_setup()
* rte_event_port_link()
* rte_event_dev_start()


Init and Config
~~~~~~~~~~~~~~~

The eventdev library uses vdev options to add devices to the DPDK application.
The ``--vdev`` EAL option allows adding eventdev instances to your DPDK
application, using the name of the eventdev PMD as an argument.

For example, to create an instance of the software eventdev scheduler, the
following vdev arguments should be provided to the application EAL command line:

.. code-block:: console

   ./dpdk_application --vdev="event_sw0"

In the following code, we configure eventdev instance with 3 queues
and 6 ports as follows. The 3 queues consist of 2 Atomic and 1 Single-Link,
while the 6 ports consist of 4 workers, 1 RX and 1 TX.

.. code-block:: c

        const struct rte_event_dev_config config = {
                .nb_event_queues = 3,
                .nb_event_ports = 6,
                .nb_events_limit  = 4096,
                .nb_event_queue_flows = 1024,
                .nb_event_port_dequeue_depth = 128,
                .nb_event_port_enqueue_depth = 128,
        };
        int err = rte_event_dev_configure(dev_id, &config);

The remainder of this walk-through assumes that dev_id is 0.

Setting up Queues
~~~~~~~~~~~~~~~~~

Once the eventdev itself is configured, the next step is to configure queues.
This is done by setting the appropriate values in a queue_conf structure, and
calling the setup function. Repeat this step for each queue, starting from
0 and ending at ``nb_event_queues - 1`` from the event_dev config above.

.. code-block:: c

        struct rte_event_queue_conf atomic_conf = {
                .schedule_type = RTE_SCHED_TYPE_ATOMIC,
                .priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
                .nb_atomic_flows = 1024,
                .nb_atomic_order_sequences = 1024,
        };
        struct rte_event_queue_conf single_link_conf = {
                .event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK,
        };
        int dev_id = 0;
        int atomic_q_1 = 0;
        int atomic_q_2 = 1;
        int single_link_q = 2;
        int err = rte_event_queue_setup(dev_id, atomic_q_1, &atomic_conf);
        int err = rte_event_queue_setup(dev_id, atomic_q_2, &atomic_conf);
        int err = rte_event_queue_setup(dev_id, single_link_q, &single_link_conf);

As shown above, queue IDs are as follows:

 * id 0, atomic queue #1
 * id 1, atomic queue #2
 * id 2, single-link queue

These queues are used for the remainder of this walk-through.

Setting up Ports
~~~~~~~~~~~~~~~~

Once queues are set up successfully, create the ports as required.

.. code-block:: c

        struct rte_event_port_conf rx_conf = {
                .dequeue_depth = 128,
                .enqueue_depth = 128,
                .new_event_threshold = 1024,
        };
        struct rte_event_port_conf worker_conf = {
                .dequeue_depth = 16,
                .enqueue_depth = 64,
                .new_event_threshold = 4096,
        };
        struct rte_event_port_conf tx_conf = {
                .dequeue_depth = 128,
                .enqueue_depth = 128,
                .new_event_threshold = 4096,
        };
        int dev_id = 0;
        int rx_port_id = 0;
        int worker_port_id;
        int err = rte_event_port_setup(dev_id, rx_port_id, &rx_conf);

        for (worker_port_id = 1; worker_port_id <= 4; worker_port_id++) {
	        int err = rte_event_port_setup(dev_id, worker_port_id, &worker_conf);
        }

        int tx_port_id = 5;
	int err = rte_event_port_setup(dev_id, tx_port_id, &tx_conf);

As shown above:

 * port 0: RX core
 * ports 1,2,3,4: Workers
 * port 5: TX core

These ports are used for the remainder of this walk-through.

Linking Queues and Ports
~~~~~~~~~~~~~~~~~~~~~~~~

The final step is to "wire up" the ports to the queues. After this, the
eventdev is capable of scheduling events, and when cores request work to do,
the correct events are provided to that core. Note that the RX core takes input
from e.g.: a NIC so it is not linked to any eventdev queues.

Linking all workers to atomic queues, and the TX core to the single-link queue
can be achieved like this:

.. code-block:: c

        uint8_t rx_port_id = 0;
        uint8_t tx_port_id = 5;
        uint8_t atomic_qs[] = {0, 1};
        uint8_t single_link_q = 2;
        uint8_t priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
        int worker_port_id;

        for (worker_port_id = 1; worker_port_id <= 4; worker_port_id++) {
                int links_made = rte_event_port_link(dev_id, worker_port_id, atomic_qs, NULL, 2);
        }
        int links_made = rte_event_port_link(dev_id, tx_port_id, &single_link_q, &priority, 1);

Linking Queues to Ports with link profiles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An application can use link profiles if supported by the underlying event device to setup up
multiple link profile per port and change them run time depending up on heuristic data.
Using Link profiles can reduce the overhead of linking/unlinking and wait for unlinks in progress
in fast-path and gives applications the ability to switch between preset profiles on the fly.

An example use case could be as follows.

Config path:

.. code-block:: c

   uint8_t lq[4] = {4, 5, 6, 7};
   uint8_t hq[4] = {0, 1, 2, 3};

   if (rte_event_dev_info.max_profiles_per_port < 2)
       return -ENOTSUP;

   rte_event_port_profile_links_set(0, 0, hq, NULL, 4, 0);
   rte_event_port_profile_links_set(0, 0, lq, NULL, 4, 1);

Worker path:

.. code-block:: c

   uint8_t profile_id_to_switch;

   while (1) {
       deq = rte_event_dequeue_burst(0, 0, &ev, 1, 0);
       if (deq == 0) {
           profile_id_to_switch = app_find_profile_id_to_switch();
           rte_event_port_profile_switch(0, 0, profile_id_to_switch);
           continue;
       }

       // Process the event received.
   }

Event Pre-scheduling
~~~~~~~~~~~~~~~~~~~~

Event pre-scheduling improves scheduling performance
by assigning events to event ports in advance when dequeues are issued.
The ``rte_event_dequeue_burst`` operation initiates the pre-schedule operation,
which completes in parallel
without affecting the dequeued event flow contexts and dequeue latency.
On the next dequeue operation, the pre-scheduled events are dequeued
and pre-schedule is initiated again.

An application can use event pre-scheduling if the event device supports it
at either device level or at a individual port level.
The application must check pre-schedule capability
by checking if ``rte_event_dev_info.event_dev_cap`` has the bit
``RTE_EVENT_DEV_CAP_PRESCHEDULE`` or ``RTE_EVENT_DEV_CAP_PRESCHEDULE_ADAPTIVE`` set,
if present pre-scheduling can be enabled at device configuration time
by setting appropriate pre-schedule type in ``rte_event_dev_config.preschedule``.

The following pre-schedule types are supported:
 * ``RTE_EVENT_PRESCHEDULE_NONE`` - No pre-scheduling.
 * ``RTE_EVENT_PRESCHEDULE`` - Always issue a pre-schedule when dequeue is issued.
 * ``RTE_EVENT_PRESCHEDULE_ADAPTIVE`` - Issue pre-schedule when dequeue is issued
   and there are no forward progress constraints.

Event devices that support ``RTE_EVENT_DEV_CAP_PER_PORT_PRESCHEDULE`` capability
allow applications to modify pre-scheduling at a per port level at runtime in fast-path.
To modify event pre-scheduling at a given event port,
the application can use ``rte_event_port_preschedule_modify()`` API.
This API can be called even if the event device does not support per port pre-scheduling,
it will be treated as a no-op.

.. code-block:: c

   rte_event_port_preschedule_modify(dev_id, port_id, RTE_EVENT_PRESCHEDULE);
   // Dequeue events from the event port with normal dequeue() function.
   rte_event_port_preschedule_modify(dev_id, port_id, RTE_EVENT_PRESCHEDULE_NONE);
   // Disable pre-scheduling if thread is about to be scheduled out
   // and issue dequeue() to drain pending events.

Application may  provide a hint to the eventdev PMD
to pre-schedule the next event without releasing the current flow context.
Event device that support this feature advertises the capability
via the ``RTE_EVENT_DEV_CAP_PRESCHEDULE_EXPLICIT`` flag.
If pre-scheduling is already enabled at a event device or event port level
or if the capability is not supported then the hint is ignored.

.. code-block:: c

   rte_event_port_preschedule(dev_id, port_id, RTE_EVENT_PRESCHEDULE);

Starting the EventDev
~~~~~~~~~~~~~~~~~~~~~

A single function call tells the eventdev instance to start processing
events. Note that all queues must be linked to for the instance to start, as
if any queue is not linked to, enqueuing to that queue will cause the
application to backpressure and eventually stall due to no space in the
eventdev.

.. code-block:: c

        int err = rte_event_dev_start(dev_id);

.. Note::

         EventDev needs to be started before starting the event producers such
         as event_eth_rx_adapter, event_timer_adapter, event_crypto_adapter,
         event_dma_adapter and event_vector_adapter.

Ingress of New Events
~~~~~~~~~~~~~~~~~~~~~

Now that the eventdev is set up, and ready to receive events, the RX core must
enqueue some events into the system for it to schedule. The events to be
scheduled are ordinary DPDK packets, received from an eth_rx_burst() as normal.
The following code shows how those packets can be enqueued into the eventdev:

.. code-block:: c

        const uint16_t nb_rx = rte_eth_rx_burst(eth_port, 0, mbufs, BATCH_SIZE);

        for (i = 0; i < nb_rx; i++) {
                ev[i].flow_id = mbufs[i]->hash.rss;
                ev[i].op = RTE_EVENT_OP_NEW;
                ev[i].sched_type = RTE_SCHED_TYPE_ATOMIC;
                ev[i].queue_id = atomic_q_1;
                ev[i].event_type = RTE_EVENT_TYPE_ETHDEV;
                ev[i].sub_event_type = 0;
                ev[i].priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
                ev[i].mbuf = mbufs[i];
        }

        const int nb_tx = rte_event_enqueue_burst(dev_id, rx_port_id, ev, nb_rx);
        if (nb_tx != nb_rx) {
                for(i = nb_tx; i < nb_rx; i++)
                        rte_pktmbuf_free(mbufs[i]);
        }

Forwarding of Events
~~~~~~~~~~~~~~~~~~~~

Now that the RX core has injected events, there is work to be done by the
workers. Note that each worker will dequeue as many events as it can in a burst,
process each one individually, and then burst the packets back into the
eventdev.

The worker can lookup the events source from ``event.queue_id``, which should
indicate to the worker what workload needs to be performed on the event.
Once done, the worker can update the ``event.queue_id`` to a new value, to send
the event to the next stage in the pipeline.

.. code-block:: c

        int timeout = 0;
        struct rte_event events[BATCH_SIZE];
        uint16_t nb_rx = rte_event_dequeue_burst(dev_id, worker_port_id, events, BATCH_SIZE, timeout);

        for (i = 0; i < nb_rx; i++) {
                /* process mbuf using events[i].queue_id as pipeline stage */
                struct rte_mbuf *mbuf = events[i].mbuf;
                /* Send event to next stage in pipeline */
                events[i].queue_id++;
        }

        uint16_t nb_tx = rte_event_enqueue_burst(dev_id, worker_port_id, events, nb_rx);


Egress of Events
~~~~~~~~~~~~~~~~

Finally, when the packet is ready for egress or needs to be dropped, we need
to inform the eventdev that the packet is no longer being handled by the
application. This can be done by calling dequeue() or dequeue_burst(), which
indicates that the previous burst of packets is no longer in use by the
application.

An event driven worker thread has following typical workflow on fastpath:

.. code-block:: c

       while (1) {
               rte_event_dequeue_burst(...);
               (event processing)
               rte_event_enqueue_burst(...);
       }

Quiescing Event Ports
~~~~~~~~~~~~~~~~~~~~~

To migrate the event port to another lcore
or while tearing down a worker core using an event port,
``rte_event_port_quiesce()`` can be invoked to make sure that all the data
associated with the event port are released from the worker core,
this might also include any prefetched events.

A flush callback can be passed to the function to handle any outstanding events.

.. code-block:: c

        rte_event_port_quiesce(dev_id, port_id, release_cb, NULL);

.. Note::

        Invocation of this API does not affect the existing port configuration.

Independent Enqueue Capability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This capability applies to eventdev devices that expects all forwarded events
to be enqueued in the same order as they are dequeued.
For dropped events, their releases should come
at the same location as the original event was expected.
The eventdev device has this restriction as it uses the order
to retrieve information about the original event that was sent to the CPU.
This contains information like atomic flow ID to release the flow lock
and ordered events sequence number to restore the original order.

This capability only matters to eventdevs supporting burst mode.
On ports where the application is going to change enqueue order,
``RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ`` support should be enabled.

Example code to inform PMD that the application plans to use
independent enqueue order on a port:

.. code-block:: c

   if (capability & RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ)
       port_config = port_config | RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ;

Stopping the EventDev
~~~~~~~~~~~~~~~~~~~~~

A single function call tells the eventdev instance to stop processing events.
A flush callback can be registered to free any inflight events
using ``rte_event_dev_stop_flush_callback_register()`` function.

.. code-block:: c

        int err = rte_event_dev_stop(dev_id);

.. Note::

        The event producers such as ``event_eth_rx_adapter``,
        ``event_timer_adapter``, ``event_crypto_adapter``,
        ``event_dma_adapter`` and ``event_vector_adapter``
        need to be stopped before stopping the event device.

Summary
-------

The eventdev library allows an application to easily schedule events as it
requires, either using a run-to-completion or pipeline processing model.  The
queues and ports abstract the logical functionality of an eventdev, providing
the application with a generic method to schedule events.  With the flexible
PMD infrastructure applications benefit of improvements in existing eventdevs
and additions of new ones without modification.
