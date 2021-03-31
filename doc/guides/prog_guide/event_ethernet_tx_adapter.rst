..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Event Ethernet Tx Adapter Library
=================================

The DPDK Eventdev API allows the application to use an event driven programming
model for packet processing in which the event device distributes events
referencing packets to the application cores in a dynamic load balanced fashion
while handling atomicity and packet ordering. Event adapters provide the interface
between the ethernet, crypto and timer devices and the event device. Event adapter
APIs enable common application code by abstracting PMD specific capabilities.
The Event ethernet Tx adapter provides configuration and data path APIs for the
transmit stage of the application allowing the same application code to use eventdev
PMD support or in its absence, a common implementation.

In the common implementation, the application enqueues mbufs to the adapter
which runs as a rte_service function. The service function dequeues events
from its event port and transmits the mbufs referenced by these events.


API Walk-through
----------------

This section will introduce the reader to the adapter API. The
application has to first instantiate an adapter which is associated with
a single eventdev, next the adapter instance is configured with Tx queues,
finally the adapter is started and the application can start enqueuing mbufs
to it.

Creating an Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An adapter instance is created using ``rte_event_eth_tx_adapter_create()``. This
function is passed the event device to be associated with the adapter and port
configuration for the adapter to setup an event port if the adapter needs to use
a service function.

If the application desires to have finer control of eventdev port configuration,
it can use the ``rte_event_eth_tx_adapter_create_ext()`` function. The
``rte_event_eth_tx_adapter_create_ext()`` function is passed a callback function.
The callback function is invoked if the adapter needs to use a service function
and needs to create an event port for it. The callback is expected to fill the
``struct rte_event_eth_tx_adapter_conf`` structure passed to it.

.. code-block:: c

        struct rte_event_dev_info dev_info;
        struct rte_event_port_conf tx_p_conf = {0};

        err = rte_event_dev_info_get(id, &dev_info);

        tx_p_conf.new_event_threshold = dev_info.max_num_events;
        tx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
        tx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;

        err = rte_event_eth_tx_adapter_create(id, dev_id, &tx_p_conf);

Adding Tx Queues to the Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ethdev Tx queues are added to the instance using the
``rte_event_eth_tx_adapter_queue_add()`` function. A queue value
of -1 is used to indicate all queues within a device.

.. code-block:: c

        int err = rte_event_eth_tx_adapter_queue_add(id,
						     eth_dev_id,
						     q);

Querying Adapter Capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_event_eth_tx_adapter_caps_get()`` function allows
the application to query the adapter capabilities for an eventdev and ethdev
combination. Currently, the only capability flag defined is
``RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT``, the application can
query this flag to determine if a service function is associated with the
adapter and retrieve its service identifier using the
``rte_event_eth_tx_adapter_service_id_get()`` API.


.. code-block:: c

        int err = rte_event_eth_tx_adapter_caps_get(dev_id, eth_dev_id, &cap);

        if (!(cap & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT))
                err = rte_event_eth_tx_adapter_service_id_get(id, &service_id);

Linking a Queue to the Adapter's Event Port
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the adapter uses a service function as described in the previous section, the
application is required to link a queue to the adapter's event port. The adapter's
event port can be obtained using the ``rte_event_eth_tx_adapter_event_port_get()``
function. The queue can be configured with the ``RTE_EVENT_QUEUE_CFG_SINGLE_LINK``
since it is linked to a single event port.

Configuring the Service Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the adapter uses a service function, the application can assign
a service core to the service function as shown below.

.. code-block:: c

        if (rte_event_eth_tx_adapter_service_id_get(id, &service_id) == 0)
                rte_service_map_lcore_set(service_id, TX_CORE_ID);

Starting the Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application calls ``rte_event_eth_tx_adapter_start()`` to start the adapter.
This function calls the start callback of the eventdev PMD if supported,
and the ``rte_service_run_state_set()`` to enable the service function if one exists.

Enqueuing Packets to the Adapter
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application needs to notify the adapter about the transmit port and queue used
to send the packet. The transmit port is set in the ``struct rte mbuf::port`` field
and the transmit queue is set using the ``rte_event_eth_tx_adapter_txq_set()``
function.

If the eventdev PMD supports the ``RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT``
capability for a given ethernet device, the application should use the
``rte_event_eth_tx_adapter_enqueue()`` function to enqueue packets to the adapter.

If the adapter uses a service function for the ethernet device then the application
should use the ``rte_event_enqueue_burst()`` function.

.. code-block:: c

	struct rte_event event;

	if (cap & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT) {

		event.mbuf = m;
		eq_flags = 0;

		m->port = tx_port;
		rte_event_eth_tx_adapter_txq_set(m, tx_queue_id);

		rte_event_eth_tx_adapter_enqueue(dev_id, ev_port, &event, 1, eq_flags);
	} else {

		event.queue_id = qid; /* event queue linked to adapter port */
		event.op = RTE_EVENT_OP_NEW;
		event.event_type = RTE_EVENT_TYPE_CPU;
		event.sched_type = RTE_SCHED_TYPE_ATOMIC;
		event.mbuf = m;

		m->port = tx_port;
		rte_event_eth_tx_adapter_txq_set(m, tx_queue_id);

		rte_event_enqueue_burst(dev_id, ev_port, &event, 1);
	}

Getting Adapter Statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~

The  ``rte_event_eth_tx_adapter_stats_get()`` function reports counters defined
in struct ``rte_event_eth_tx_adapter_stats``. The counter values are the sum of
the counts from the eventdev PMD callback if the callback is supported, and
the counts maintained by the service function, if one exists.

Tx event vectorization
~~~~~~~~~~~~~~~~~~~~~~

The event device, ethernet device pairs which support the capability
``RTE_EVENT_ETH_TX_ADAPTER_CAP_EVENT_VECTOR`` can process event vector of mbufs.
Additionally, application can provide a hint to the Tx adapter that all the
mbufs are destined to the same ethernet port and queue by setting the bit
``rte_event_vector::attr_valid`` and filling `rte_event_vector::port`` and
``rte_event_vector::queue``.
If ``rte_event_vector::attr_valid`` is not set then the Tx adapter should peek
into each mbuf and transmit them to the requested ethernet port and queue pair.
