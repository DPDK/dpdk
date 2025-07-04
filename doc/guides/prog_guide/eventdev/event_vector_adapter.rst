.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2025 Marvell International Ltd.

Event Vector Adapter Library
============================

The event vector adapter library enhances the event-driven model
by enabling the CPU to offload the creation, aggregation and queuing
of event vectors (to a configurable event queue)
thereby reducing the CPU's workload related to vector allocation and aggregation.

Event vectors are created by aggregating multiple 8B objects
(e.g. ``rte_event_vector::mbufs``, ``rte_event_vector::u64s``)
into a ``rte_event_vector``.
Currently, the Rx adapter and crypto adapter support
offloading vector creation and aggregation to the event device.

The event vector adapter library is designed to allow CPU to enqueue objects
that have to be aggregated to underlying hardware
or software implementation of vector aggregator.
The library queries an eventdev PMD to determine the appropriate implementation
and provides API to create, configure, and manage vector adapters.

Examples of using the API are presented in the `API Overview`_
and `Processing Vector Events`_ sections.


Behavior
--------

Vector Event
~~~~~~~~~~~~

A vector event is enqueued in the event device
when the vector adapter reaches the configured vector size or timeout.
The event device uses the attributes configured by the application when scheduling it.

Fallback
~~~~~~~~

If the vector adapter cannot aggregate objects into a vector event,
it enqueues the objects as single events
with fallback event properties configured by the application.

Timeout and Size
~~~~~~~~~~~~~~~~

The vector adapter aggregates objects
until the configured vector size or timeout is reached.
If the timeout is reached before the minimum vector size is met,
the adapter enqueues the objects as single events
with fallback event properties configured by the application.


API Overview
------------

This section introduces the event vector adapter API,
showing how to create and configure a vector adapter
and use it to manage vector events.

From a high level, the setup steps are:

* ``rte_event_vector_adapter_create()``

And to enqueue and manage vectors:

* ``rte_event_vector_adapter_enqueue()``
* ``rte_event_vector_adapter_stats_get()``

Create and Configure a Vector Adapter
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To create a vector adapter instance,
initialize an ``rte_event_vector_adapter_conf`` struct with the desired values,
and pass it to ``rte_event_vector_adapter_create()``.

.. code-block:: c

	const struct rte_event_vector_adapter_conf adapter_config = {
		.event_dev_id = event_dev_id,
		.socket_id = rte_socket_id(),
		.ev = {
			.queue_id = event_queue_id,
			.sched_type = RTE_SCHED_TYPE_ATOMIC,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.event_type = RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU,
		},
		.ev_fallback = {
			.event_type = RTE_EVENT_TYPE_CPU,
		},
		.vector_sz = 64,
		.vector_timeout_ns = 1000000, // 1ms
		.vector_mp = vector_mempool,
	};

	struct rte_event_vector_adapter *adapter;
	adapter = rte_event_vector_adapter_create(&adapter_config);

	if (adapter == NULL) { ... }

Before creating an instance of a vector adapter,
the application should create and configure an event device
along with its event ports.
Based on the event device's capability,
it might require creating an additional event port
to be used by the vector adapter.
If required, the ``rte_event_vector_adapter_create()`` function
will use a default method to configure an event port.

If the application desires finer control of event port allocation and setup,
it can use the ``rte_event_vector_adapter_create_ext()`` function.
This function is passed a callback function
that will be invoked if the adapter needs to create an event port,
giving the application the opportunity to control how it is done.

Retrieve Vector Adapter Contextual Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The vector adapter implementation may have constraints
on vector size or timeout based on the given event device or system.
The application can retrieve these constraints
using ``rte_event_vector_adapter_info_get()``.
This function returns an ``rte_event_vector_adapter_info`` struct,
which contains the following members:

``max_vector_adapters_per_event_queue``
  Maximum number of vector adapters configurable per event queue.
``min_vector_sz``
  Minimum vector size configurable.
``max_vector_sz``
  Maximum vector size configurable.
``min_vector_timeout_ns``
  Minimum vector timeout configurable.
``max_vector_timeout_ns``
  Maximum vector timeout configurable.
``log2_sz``
  Vector size should be a power of 2.

Enqueuing Objects to the Vector Adapter
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once a vector adapter has been created,
the application can enqueue objects to it
using ``rte_event_vector_adapter_enqueue()``.
The adapter will aggregate the objects into a vector event
based on the configured size and timeout.

.. code-block:: c

	uint64_t objs[32];
	uint16_t num_elem = 32;
	uint64_t flags = 0;

	int ret = rte_event_vector_adapter_enqueue(adapter, objs, num_elem, flags);
	if (ret < 0) { ... }

The application can use
the ``RTE_EVENT_VECTOR_ENQ_SOV`` and ``RTE_EVENT_VECTOR_ENQ_EOV`` flags
to control the start and end of vector aggregation
if the vector adapter supports ``RTE_EVENT_VECTOR_ADAPTER_CAP_SOV_EOV`` capability;
if not, then the flags will be ignored.

The ``RTE_EVENT_VECTOR_ENQ_SOV`` flag marks the beginning of a vector
and applies to the first pointer in the enqueue operation.
Any incomplete vectors will be enqueued to the event device.

The ``RTE_EVENT_VECTOR_ENQ_EOV`` flag marks the end of a vector
and applies to the last pointer in the enqueue operation.
The vector is enqueued to the event device
even if the configured vector size is not reached.

If both flags are set, the adapter will form a new vector event
with the given objects and enqueue it to the event device.

The ``RTE_EVENT_VECTOR_ENQ_FLUSH`` flag can be used to flush
any remaining objects in the vector adapter.
This is useful when the application needs to ensure that all objects are processed,
even if the configured vector size or timeout is not reached.
An enqueue call with this flag set will not handle any objects and will return 0.


Processing Vector Events
~~~~~~~~~~~~~~~~~~~~~~~~

Once a vector event has been enqueued in the event device,
the application will subsequently dequeue it from the event device.
The application can process the vector event and its aggregated objects as needed:

.. code-block:: c

	void
	event_processing_loop(...)
	{
		while (...) {
			/* Receive events from the configured event port. */
			rte_event_dequeue_burst(event_dev_id, event_port, &ev, 1, 0);
			...
			switch(ev.event_type) {
				...
				case RTE_EVENT_TYPE_VECTOR:
					process_vector_event(ev);
					...
					break;
			}
		}
	}

	void
	process_vector_event(struct rte_event ev)
	{
		struct rte_event_vector *vector = ev.event_ptr;
		for (uint16_t i = 0; i < vector->nb_elem; i++) {
			uint64_t obj = vector->u64s[i];
			/* Process each object in the vector. */
			...
		}
	}


Statistics and Cleanup
~~~~~~~~~~~~~~~~~~~~~~

The application can retrieve statistics for the vector adapter
using ``rte_event_vector_adapter_stats_get()``.

.. code-block:: c

	struct rte_event_vector_adapter_stats stats;
	rte_event_vector_adapter_stats_get(adapter, &stats);

	printf("Vectors created: %" PRIu64 "\n", stats.vectorized);
	printf("Timeouts occurred: %" PRIu64 "\n", stats.vectors_timedout);

To reset the statistics, use ``rte_event_vector_adapter_stats_reset()``.

To destroy the vector adapter and release its resources,
use ``rte_event_vector_adapter_destroy()``.
The destroy function will flush any remaining events
in the vector adapter before destroying it.
