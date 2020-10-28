..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Event Ethernet Rx Adapter Library
=================================

The DPDK Eventdev API allows the application to use an event driven programming
model for packet processing. In this model, the application polls an event
device port for receiving events that reference packets instead of polling Rx
queues of ethdev ports. Packet transfer between ethdev and the event device can
be supported in hardware or require a software thread to receive packets from
the ethdev port using ethdev poll mode APIs and enqueue these as events to the
event device using the eventdev API. Both transfer mechanisms may be present on
the same platform depending on the particular combination of the ethdev and
the event device.

The Event Ethernet Rx Adapter library is intended for the application code to
configure both transfer mechanisms using a common API. A capability API allows
the eventdev PMD to advertise features supported for a given ethdev and allows
the application to perform configuration as per supported features.

API Walk-through
----------------

This section will introduce the reader to the adapter API. The
application has to first instantiate an adapter which is associated with
a single eventdev, next the adapter instance is configured with Rx queues
that are either polled by a SW thread or linked using hardware support. Finally
the adapter is started.

For SW based packet transfers from ethdev to eventdev, the adapter uses a
DPDK service function and the application is also required to assign a core to
the service function.

Creating an Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An adapter instance is created using ``rte_event_eth_rx_adapter_create()``. This
function is passed the event device to be associated with the adapter and port
configuration for the adapter to setup an event port if the adapter needs to use
a service function.

.. code-block:: c

        int err;
        uint8_t dev_id;
        struct rte_event_dev_info dev_info;
        struct rte_event_port_conf rx_p_conf;

        err = rte_event_dev_info_get(id, &dev_info);

        rx_p_conf.new_event_threshold = dev_info.max_num_events;
        rx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
        rx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;
        err = rte_event_eth_rx_adapter_create(id, dev_id, &rx_p_conf);

If the application desires to have finer control of eventdev port allocation
and setup, it can use the ``rte_event_eth_rx_adapter_create_ext()`` function.
The ``rte_event_eth_rx_adapter_create_ext()`` function is passed a callback
function. The callback function is invoked if the adapter needs to use a
service function and needs to create an event port for it. The callback is
expected to fill the ``struct rte_event_eth_rx_adapter_conf structure``
passed to it.

Adding Rx Queues to the Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ethdev Rx queues are added to the instance using the
``rte_event_eth_rx_adapter_queue_add()`` function. Configuration for the Rx
queue is passed in using a ``struct rte_event_eth_rx_adapter_queue_conf``
parameter. Event information for packets from this Rx queue is encoded in the
``ev`` field of ``struct rte_event_eth_rx_adapter_queue_conf``. The
servicing_weight member of the struct  rte_event_eth_rx_adapter_queue_conf
is the relative polling frequency of the Rx queue and is applicable when the
adapter uses a service core function.

.. code-block:: c

        ev.queue_id = 0;
        ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
        ev.priority = 0;

        queue_config.rx_queue_flags = 0;
        queue_config.ev = ev;
        queue_config.servicing_weight = 1;

        err = rte_event_eth_rx_adapter_queue_add(id,
                                                eth_dev_id,
                                                0, &queue_config);

Querying Adapter Capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_event_eth_rx_adapter_caps_get()`` function allows
the application to query the adapter capabilities for an eventdev and ethdev
combination. For e.g, if the ``RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID``
is set, the application can override the adapter generated flow ID in the event
using ``rx_queue_flags`` field in ``struct rte_event_eth_rx_adapter_queue_conf``
which is passed as a parameter to the ``rte_event_eth_rx_adapter_queue_add()``
function.

.. code-block:: c

        err = rte_event_eth_rx_adapter_caps_get(dev_id, eth_dev_id, &cap);

        queue_config.rx_queue_flags = 0;
        if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
                ev.flow_id = 1;
                queue_config.rx_queue_flags =
                        RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
        }

Configuring the Service Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the adapter uses a service function, the application is required to assign
a service core to the service function as show below.

.. code-block:: c

        uint32_t service_id;

        if (rte_event_eth_rx_adapter_service_id_get(0, &service_id) == 0)
                rte_service_map_lcore_set(service_id, RX_CORE_ID);

Starting the Adapter Instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application calls ``rte_event_eth_rx_adapter_start()`` to start the adapter.
This function calls the start callbacks of the eventdev PMDs for hardware based
eventdev-ethdev connections and ``rte_service_run_state_set()`` to enable the
service function if one exists.

.. Note::

         The eventdev to which the event_eth_rx_adapter is connected needs to
         be started before calling rte_event_eth_rx_adapter_start().

Getting Adapter Statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~

The  ``rte_event_eth_rx_adapter_stats_get()`` function reports counters defined
in struct ``rte_event_eth_rx_adapter_stats``. The received packet and
enqueued event counts are a sum of the counts from the eventdev PMD callbacks
if the callback is supported, and the counts maintained by the service function,
if one exists. The service function also maintains a count of cycles for which
it was not able to enqueue to the event device.

Interrupt Based Rx Queues
~~~~~~~~~~~~~~~~~~~~~~~~~~

The service core function is typically set up to poll ethernet Rx queues for
packets. Certain queues may have low packet rates and it would be more
efficient to enable the Rx queue interrupt and read packets after receiving
the interrupt.

The servicing_weight member of struct rte_event_eth_rx_adapter_queue_conf
is applicable when the adapter uses a service core function. The application
has to enable Rx queue interrupts when configuring the ethernet device
using the ``rte_eth_dev_configure()`` function and then use a servicing_weight
of zero when adding the Rx queue to the adapter.

The adapter creates a thread blocked on the interrupt, on an interrupt this
thread enqueues the port id and the queue id to a ring buffer. The adapter
service function dequeues the port id and queue id from the ring buffer,
invokes the ``rte_eth_rx_burst()`` to receive packets on the queue and
converts the received packets to events in the same manner as packets
received on a polled Rx queue. The interrupt thread is affinitized to the same
CPUs as the lcores of the Rx adapter service function, if the Rx adapter
service function has not been mapped to any lcores, the interrupt thread
is mapped to the main lcore.

Rx Callback for SW Rx Adapter
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For SW based packet transfers, i.e., when the
``RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT`` is not set in the adapter's
capabilities flags for a particular ethernet device, the service function
temporarily enqueues mbufs to an event buffer before batch enqueuing these
to the event device. If the buffer fills up, the service function stops
dequeuing packets from the ethernet device. The application may want to
monitor the buffer fill level and instruct the service function to selectively
enqueue packets to the event device. The application may also use some other
criteria to decide which packets should enter the event device even when
the event buffer fill level is low. The
``rte_event_eth_rx_adapter_cb_register()`` function allow the application
to register a callback that selects which packets to enqueue to the event
device.
