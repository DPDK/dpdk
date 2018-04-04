..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation. All rights reserved.

Event Timer Adapter Library
===========================

The DPDK
`Event Device library <http://dpdk.org/doc/guides/prog_guide/eventdev.html>`_
introduces an event driven programming model which presents applications with
an alternative to the polling model traditionally used in DPDK
applications. Event devices can be coupled with arbitrary components to provide
new event sources by using **event adapters**. The Event Timer Adapter is one
such adapter; it bridges event devices and timer mechanisms.

The Event Timer Adapter library extends the event driven model
by introducing a :ref:`new type of event <timer_expiry_event>` that represents
a timer expiration, and providing an API with which adapters can be created or
destroyed, and :ref:`event timers <event_timer>` can be armed and canceled.

The Event Timer Adapter library is designed to interface with hardware or
software implementations of the timer mechanism; it will query an eventdev PMD
to determine which implementation should be used.  The default software
implementation manages timers using the DPDK
`Timer library <http://dpdk.org/doc/guides/prog_guide/timer_lib.html>`_.

Examples of using the API are presented in the `API Overview`_ and
`Processing Timer Expiry Events`_ sections.  Code samples are abstracted and
are based on the example of handling a TCP retransmission.

.. _event_timer:

Event Timer struct
------------------
Event timers are timers that enqueue a timer expiration event to an event
device upon timer expiration.

The Event Timer Adapter API represents each event timer with a generic struct,
which contains an event and user metadata.  The ``rte_event_timer`` struct is
defined in ``lib/librte_event/librte_event_timer_adapter.h``.

.. _timer_expiry_event:

Timer Expiry Event
~~~~~~~~~~~~~~~~~~

The event contained by an event timer is enqueued in the event device when the
timer expires, and the event device uses the attributes below when scheduling
it:

* ``event_queue_id`` - Application should set this to specify an event queue to
  which the timer expiry event should be enqueued
* ``event_priority`` - Application can set this to indicate the priority of the
  timer expiry event in the event queue relative to other events
* ``sched_type`` - Application can set this to specify the scheduling type of
  the timer expiry event
* ``flow_id`` - Application can set this to indicate which flow this timer
  expiry event corresponds to
* ``op`` - Will be set to ``RTE_EVENT_OP_NEW`` by the event timer adapter
* ``event_type`` - Will be set to ``RTE_EVENT_TYPE_TIMER`` by the event timer
  adapter

Timeout Ticks
~~~~~~~~~~~~~

The number of ticks from now in which the timer will expire. The ticks value
has a resolution (``timer_tick_ns``) that is specified in the event timer
adapter configuration.

State
~~~~~

Before arming an event timer, the application should initialize its state to
RTE_EVENT_TIMER_NOT_ARMED. The event timer's state will be updated when a
request to arm or cancel it takes effect.

If the application wishes to rearm the timer after it has expired, it should
reset the state back to RTE_EVENT_TIMER_NOT_ARMED before doing so.

User Metadata
~~~~~~~~~~~~~

Memory to store user specific metadata.  The event timer adapter implementation
will not modify this area.

API Overview
------------

This section will introduce the reader to the event timer adapter API, showing
how to create and configure an event timer adapter and use it to manage event
timers.

From a high level, the setup steps are:

* rte_event_timer_adapter_create()
* rte_event_timer_adapter_start()

And to start and stop timers:

* rte_event_timer_arm_burst()
* rte_event_timer_cancel_burst()

Create and Configure an Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To create an event timer adapter instance, initialize an
``rte_event_timer_adapter_conf`` struct with the desired values, and pass it
to ``rte_event_timer_adapter_create()``.

.. code-block:: c

	#define NSECPERSEC 1E9 // No of ns in 1 sec
	const struct rte_event_timer_adapter_conf adapter_config = {
                .event_dev_id = event_dev_id,
                .timer_adapter_id = 0,
                .clk_src = RTE_EVENT_TIMER_ADAPTER_CPU_CLK,
                .timer_tick_ns = NSECPERSEC / 10, // 100 milliseconds
                .max_tmo_nsec = 180 * NSECPERSEC // 2 minutes
                .nb_timers = 40000,
                .timer_adapter_flags = 0,
	};

	struct rte_event_timer_adapter *adapter = NULL;
	adapter = rte_event_timer_adapter_create(&adapter_config);

	if (adapter == NULL) { ... };

Before creating an instance of a timer adapter, the application should create
and configure an event device along with its event ports. Based on the event
device capability, it might require creating an additional event port to be
used by the timer adapter.  If required, the
``rte_event_timer_adapter_create()`` function will use a default method to
configure an event port;  it will examine the current event device
configuration, determine the next available port identifier number, and create
a new event port with a default port configuration.

If the application desires to have finer control of event port allocation
and setup, it can use the ``rte_event_timer_adapter_create_ext()`` function.
This function is passed a callback function that will be invoked if the
adapter needs to create an event port, giving the application the opportunity
to control how it is done.

Retrieve Event Timer Adapter Contextual Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The event timer adapter implementation may have constraints on tick resolution
or maximum timer expiry timeout based on the given event timer adapter or
system.  In this case, the implementation may adjust the tick resolution or
maximum timeout to the best possible configuration.

Upon successful event timer adapter creation, the application can get the
configured resolution and max timeout with
``rte_event_timer_adapter_get_info()``. This function will return an
``rte_event_timer_adapter_info`` struct, which contains the following members:

* ``min_resolution_ns`` - Minimum timer adapter tick resolution in ns.
* ``max_tmo_ns`` - Maximum timer timeout(expiry) in ns.
* ``adapter_conf`` - Configured event timer adapter attributes

Configuring the Service Component
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the adapter uses a service component, the application is required to map
the service to a service core before starting the adapter:

.. code-block:: c

        uint32_t service_id;

        if (rte_event_timer_adapter_service_id_get(adapter, &service_id) == 0)
                rte_service_map_lcore_set(service_id, EVTIM_CORE_ID);

An event timer adapter uses a service component if the event device PMD
indicates that the adapter should use a software implementation.

Starting the Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application should call ``rte_event_timer_adapter_start()`` to start
running the event timer adapter. This function calls the start entry points
defined by eventdev PMDs for hardware implementations or puts a service
component into the running state in the software implementation.

Arming Event Timers
~~~~~~~~~~~~~~~~~~~

Once an event timer adapter has been started, an application can begin to
manage event timers with it.

The application should allocate ``struct rte_event_timer`` objects from a
mempool or huge-page backed application buffers of required size. Upon
successful allocation, the application should initialize the event timer, and
then set any of the necessary event attributes described in the
`Timer Expiry Event`_ section. In the following example, assume ``conn``
represents a TCP connection and that ``event_timer_pool`` is a mempool that
was created previously:

.. code-block:: c

	rte_mempool_get(event_timer_pool, (void **)&conn->evtim);
	if (conn->evtim == NULL) { ... }

	/* Set up the event timer. */
	conn->evtim->ev.op = RTE_EVENT_OP_NEW;
	conn->evtim->ev.queue_id = event_queue_id;
        conn->evtim->ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
        conn->evtim->ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
        conn->evtim->ev.event_type = RTE_EVENT_TYPE_TIMER;
	conn->evtim->ev.event_ptr = conn;
	conn->evtim->state = RTE_EVENT_TIMER_NOT_ARMED;
	conn->evtim->timeout_ticks = 30; //3 sec Per RFC1122(TCP returns)

Note that it is necessary to initialize the event timer state to
RTE_EVENT_TIMER_NOT_ARMED.  Also note that we have saved a pointer to the
``conn`` object in the timer's event payload. This will allow us to locate
the connection object again once we dequeue the timer expiry event from the
event device later.  As a convenience, the application may specify no value for
ev.event_ptr, and the adapter will by default set it to point at the event
timer itself.

Now we can arm the event timer with ``rte_event_timer_arm_burst()``:

.. code-block:: c

	ret = rte_event_timer_arm_burst(adapter, &conn->evtim, 1);
	if (ret != 1) { ... }

Once an event timer expires, the application may free it or rearm it as
necessary.  If the application will rearm the timer, the state should be reset
to RTE_EVENT_TIMER_NOT_ARMED by the application before rearming it.

Multiple Event Timers with Same Expiry Value
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the special case that there is a set of event timers that should all expire
at the same time, the application may call
``rte_event_timer_arm_tmo_tick_burst()``, which allows the implementation to
optimize the operation if possible.

Canceling Event Timers
~~~~~~~~~~~~~~~~~~~~~~

An event timer that has been armed as described in `Arming Event Timers`_ can
be canceled by calling ``rte_event_timer_cancel_burst()``:

.. code-block:: c

	/* Ack for the previous tcp data packet has been received;
	 * cancel the retransmission timer
         */
	rte_event_timer_cancel_burst(adapter, &conn->timer, 1);

Processing Timer Expiry Events
------------------------------

Once an event timer has successfully enqueued a timer expiry event in the event
device, the application will subsequently dequeue it from the event device.
The application can use the event payload to retrieve a pointer to the object
associated with the event timer. It can then re-arm the event timer or free the
event timer object as desired:

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
				case RTE_EVENT_TYPE_TIMER:
					process_timer_event(ev);
					...
					break;
			}
		}
	}

	uint8_t
	process_timer_event(...)
	{
		/* A retransmission timeout for the connection has been received. */
		conn = ev.event_ptr;
		/* Retransmit last packet (e.g. TCP segment). */
		...
		/* Re-arm timer using original values. */
		rte_event_timer_arm_burst(adapter_id, &conn->timer, 1);
	}

Summary
-------

The Event Timer Adapter library extends the DPDK event-based programming model
by representing timer expirations as events in the system and allowing
applications to use existing event processing loops to arm and cancel event
timers or handle timer expiry events.
