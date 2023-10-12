..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2023 Ericsson AB.

Dispatcher Library
==================

Overview
--------

The purpose of the dispatcher is to help reduce coupling in an
:doc:`Eventdev <eventdev>`-based DPDK application.

In particular, the dispatcher addresses a scenario where an
application's modules share the same event device and event device
ports, and performs work on the same lcore threads.

The dispatcher replaces the conditional logic that follows an event
device dequeue operation, where events are dispatched to different
parts of the application, typically based on fields in the
``rte_event``, such as the ``queue_id``, ``sub_event_type``, or
``sched_type``.

Below is an excerpt from a fictitious application consisting of two
modules; A and B. In this example, event-to-module routing is based
purely on queue id, where module A expects all events to a certain
queue id, and module B two other queue ids.

.. note::

   Event routing may reasonably be done based on other ``rte_event``
   fields (or even event user data). Indeed, that's the very reason to
   have match callback functions, instead of a simple queue
   id-to-handler mapping scheme. Queue id-based routing serves well in
   a simple example.

.. code-block:: c

    for (;;) {
            struct rte_event events[MAX_BURST];
            unsigned int n;

            n = rte_event_dequeue_burst(dev_id, port_id, events,
	                                MAX_BURST, 0);

            for (i = 0; i < n; i++) {
                    const struct rte_event *event = &events[i];

                    switch (event->queue_id) {
                    case MODULE_A_QUEUE_ID:
                            module_a_process(event);
                            break;
                    case MODULE_B_STAGE_0_QUEUE_ID:
                            module_b_process_stage_0(event);
                            break;
                    case MODULE_B_STAGE_1_QUEUE_ID:
                            module_b_process_stage_1(event);
                            break;
                    }
            }
    }

The issue this example attempts to illustrate is that the centralized
conditional logic has knowledge of things that should be private to
the modules. In other words, this pattern leads to a violation of
module encapsulation.

The shared conditional logic contains explicit knowledge about what
events should go where. In case, for example, the
``module_a_process()`` is broken into two processing stages — a
module-internal affair — the shared conditional code must be updated
to reflect this change.

The centralized event routing code becomes an issue in larger
applications, where modules are developed by different organizations.
This pattern also makes module reuse across different applications more
difficult. The part of the conditional logic relevant for a particular
application may need to be duplicated across many module
instantiations (e.g., applications and test setups).

The dispatcher separates the mechanism (routing events to their
receiver) from the policy (which events should go where).

The basic operation of the dispatcher is as follows:

* Dequeue a batch of events from the event device.
* For each event determine which handler should receive the event, using
  a set of application-provided, per-handler event matching callback
  functions.
* Provide events matching a particular handler, to that handler, using
  its process callback.

If the above application would have made use of the dispatcher, the
code relevant for its module A may have looked something like this:

.. code-block:: c

    static bool
    module_a_match(const struct rte_event *event, void *cb_data)
    {
           return event->queue_id == MODULE_A_QUEUE_ID;
    }

    static void
    module_a_process_events(uint8_t event_dev_id, uint8_t event_port_id,
                            const struct rte_event *events,
			    uint16_t num, void *cb_data)
    {
            uint16_t i;

            for (i = 0; i < num; i++)
                    module_a_process_event(&events[i]);
    }

    /* In the module's initialization code */
    rte_dispatcher_register(dispatcher, module_a_match, NULL,
			    module_a_process_events, module_a_data);

.. note::

   Error handling is left out of this and future example code in this chapter.

When the shared conditional logic is removed, a new question arises:
which part of the system actually runs the dispatching mechanism? Or
phrased differently, what is replacing the function hosting the shared
conditional logic (typically launched on all lcores using
``rte_eal_remote_launch()``)? To solve this issue, the dispatcher is
run as a DPDK :doc:`Service <service_cores>`.

The dispatcher is a layer between the application and the event device
in the receive direction. In the transmit (i.e., item of work
submission) direction, the application directly accesses the Eventdev
core API (e.g., ``rte_event_enqueue_burst()``) to submit new or
forwarded events to the event device.

Dispatcher Creation
-------------------

A dispatcher is created using the ``rte_dispatcher_create()`` function.

The event device must be configured before the dispatcher is created.

Usually, only one dispatcher is needed per event device. A dispatcher
handles exactly one event device.

A dispatcher is freed using the ``rte_dispatcher_free()`` function.
The dispatcher's service functions must not be running on
any lcore at the point of this call.

Event Port Binding
------------------

To be able to dequeue events, the dispatcher must know which event
ports are to be used, on all the lcores it uses. The application
provides this information using
``rte_dispatcher_bind_port_to_lcore()``.

This call is typically made from the part of the application that
deals with deployment issues (e.g., iterating lcores and determining
which lcore does what), at the time of application initialization.

The ``rte_dispatcher_unbind_port_from_lcore()`` is used to undo
this operation.

Multiple lcore threads may not safely use the same event
port.

.. note::

   This property (which is a feature, not a bug) is inherited from the
   core Eventdev APIs.

Event ports cannot safely be bound or unbound while the dispatcher's
service function is running on any lcore.

Event Handlers
--------------

The dispatcher handler is an interface between the dispatcher and an
application module, used to route events to the appropriate part of
the application.

Handler Registration
^^^^^^^^^^^^^^^^^^^^

The event handler interface consists of two function pointers:

* The ``rte_dispatcher_match_t`` callback, which job is to
  decide if this event is to be the property of this handler.
* The ``rte_dispatcher_process_t``, which is used by the
  dispatcher to deliver matched events.

An event handler registration is valid on all lcores.

The functions pointed to by the match and process callbacks resides in
the application's domain logic, with one or more handlers per
application module.

A module may use more than one event handler, for convenience or to
further decouple sub-modules. However, the dispatcher may impose an
upper limit of the number of handlers. In addition, installing a large
number of handlers increase dispatcher overhead, although this does
not necessarily translate to a system-level performance degradation. See
the section on :ref:`Event Clustering` for more information.

Handler registration and unregistration cannot safely be done while
the dispatcher's service function is running on any lcore.

Event Matching
^^^^^^^^^^^^^^

A handler's match callback function decides if an event should be
delivered to this handler, or not.

An event is routed to no more than one handler. Thus, if a match
function returns true, no further match functions will be invoked for
that event.

Match functions must not depend on being invocated in any particular
order (e.g., in the handler registration order).

Events failing to match any handler are dropped, and the
``ev_drop_count`` counter is updated accordingly.

Event Delivery
^^^^^^^^^^^^^^

The handler callbacks are invocated by the dispatcher's service
function, upon the arrival of events to the event ports bound to the
running service lcore.

A particular event is delivered to at most one handler.

The application must not depend on all match callback invocations for
a particular event batch being made prior to any process calls are
being made. For example, if the dispatcher dequeues two events from
the event device, it may choose to find out the destination for the
first event, and deliver it, and then continue to find out the
destination for the second, and then deliver that event as well. The
dispatcher may also choose a strategy where no event is delivered
until the destination handler for both events have been determined.

The events provided in a single process call always belong to the same
event port dequeue burst.

.. _Event Clustering:

Event Clustering
^^^^^^^^^^^^^^^^

The dispatcher maintains the order of events destined for the same
handler.

*Order* here refers to the order in which the events were delivered
from the event device to the dispatcher (i.e., in the event array
populated by ``rte_event_dequeue_burst()``), in relation to the order
in which the dispatcher delivers these events to the application.

The dispatcher *does not* guarantee to maintain the order of events
delivered to *different* handlers.

For example, assume that ``MODULE_A_QUEUE_ID`` expands to the value 0,
and ``MODULE_B_STAGE_0_QUEUE_ID`` expands to the value 1. Then
consider a scenario where the following events are dequeued from the
event device (qid is short for event queue id).

.. code-block:: none

    [e0: qid=1], [e1: qid=1], [e2: qid=0], [e3: qid=1]

The dispatcher may deliver the events in the following manner:

.. code-block:: none

   module_b_stage_0_process([e0: qid=1], [e1: qid=1])
   module_a_process([e2: qid=0])
   module_b_stage_0_process([e2: qid=1])

The dispatcher may also choose to cluster (group) all events destined
for ``module_b_stage_0_process()`` into one array:

.. code-block:: none

   module_b_stage_0_process([e0: qid=1], [e1: qid=1], [e3: qid=1])
   module_a_process([e2: qid=0])

Here, the event ``e2`` is reordered and placed behind ``e3``, from a
delivery order point of view. This kind of reshuffling is allowed,
since the events are destined for different handlers.

The dispatcher may also deliver ``e2`` before the three events
destined for module B.

An example of what the dispatcher may not do, is to reorder event
``e1`` so, that it precedes ``e0`` in the array passed to the module
B's stage 0 process callback.

Although clustering requires some extra work for the dispatcher, it
leads to fewer process function calls. In addition, and likely more
importantly, it improves temporal locality of memory accesses to
handler-specific data structures in the application, which in turn may
lead to fewer cache misses and improved overall performance.

Finalize
--------

The dispatcher may be configured to notify one or more parts of the
application when the matching and processing of a batch of events has
completed.

The ``rte_dispatcher_finalize_register`` call is used to
register a finalize callback. The function
``rte_dispatcher_finalize_unregister`` is used to remove a
callback.

The finalize hook may be used by a set of event handlers (in the same
modules, or a set of cooperating modules) sharing an event output
buffer, since it allows for flushing of the buffers at the last
possible moment. In particular, it allows for buffering of
``RTE_EVENT_OP_FORWARD`` events, which must be flushed before the next
``rte_event_dequeue_burst()`` call is made (assuming implicit release
is employed).

The following is an example with an application-defined event output
buffer (the ``event_buffer``):

.. code-block:: c

    static void
    finalize_batch(uint8_t event_dev_id, uint8_t event_port_id,
                   void *cb_data)
    {
            struct event_buffer *buffer = cb_data;
            unsigned lcore_id = rte_lcore_id();
            struct event_buffer_lcore *lcore_buffer =
                    &buffer->lcore_buffer[lcore_id];

            event_buffer_lcore_flush(lcore_buffer);
    }

    /* In the module's initialization code */
    rte_dispatcher_finalize_register(dispatcher, finalize_batch,
                                     shared_event_buffer);

The dispatcher does not track any relationship between a handler and a
finalize callback, and all finalize callbacks will be called, if (and
only if) at least one event was dequeued from the event device.

Finalize callback registration and unregistration cannot safely be
done while the dispatcher's service function is running on any lcore.

Service
-------

The dispatcher is a DPDK service, and is managed in a manner similar
to other DPDK services (e.g., an Event Timer Adapter).

Below is an example of how to configure a particular lcore to serve as
a service lcore, and to map an already-configured dispatcher
(identified by ``DISPATCHER_ID``) to that lcore.

.. code-block:: c

    static void
    launch_dispatcher_core(struct rte_dispatcher *dispatcher,
                           unsigned lcore_id)
    {
            uint32_t service_id;

            rte_service_lcore_add(lcore_id);

            rte_dispatcher_service_id_get(dispatcher, &service_id);

            rte_service_map_lcore_set(service_id, lcore_id, 1);

            rte_service_lcore_start(lcore_id);

            rte_service_runstate_set(service_id, 1);
    }

As the final step, the dispatcher must be started.

.. code-block:: c

    rte_dispatcher_start(dispatcher);


Multi Service Dispatcher Lcores
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In an Eventdev application, most (or all) compute-intensive and
performance-sensitive processing is done in an event-driven manner,
where CPU cycles spent on application domain logic is the direct
result of items of work (i.e., ``rte_event`` events) dequeued from an
event device.

In the light of this, it makes sense to have the dispatcher service be
the only DPDK service on all lcores used for packet processing — at
least in principle.

However, there is nothing in DPDK that prevents colocating other
services with the dispatcher service on the same lcore.

Tasks that prior to the introduction of the dispatcher into the
application was performed on the lcore, even though no events were
received, are prime targets for being converted into such auxiliary
services, running on the dispatcher core set.

An example of such a task would be the management of a per-lcore timer
wheel (i.e., calling ``rte_timer_manage()``).

Applications employing :doc:`Read-Copy-Update (RCU) <rcu_lib>` (or
similar technique) may opt for having quiescent state (e.g., calling
``rte_rcu_qsbr_quiescent()``) signaling factored out into a separate
service, to assure resource reclaiming occurs even though some
lcores currently do not process any events.

If more services than the dispatcher service is mapped to a service
lcore, it's important that the other service are well-behaved and
don't interfere with event processing to the extent the system's
throughput and/or latency requirements are at risk of not being met.

In particular, to avoid jitter, they should have a small upper bound
for the maximum amount of time spent in a single service function
call.

An example of scenario with a more CPU-heavy colocated service is a
low-lcore count deployment, where the event device lacks the
``RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT`` capability (and thus
requires software to feed incoming packets into the event device). In
this case, the best performance may be achieved if the Event Ethernet
RX and/or TX Adapters are mapped to lcores also used for event
dispatching, since otherwise the adapter lcores would have a lot of
idle CPU cycles.
