..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation. All rights reserved.

Event Crypto Adapter Library
============================

The DPDK :doc:`Eventdev library <eventdev>` provides event driven
programming model with features to schedule events.
The :doc:`Cryptodev library <cryptodev_lib>` provides an interface to
the crypto poll mode drivers which supports different crypto operations.
The Event Crypto Adapter is one of the adapter which is intended to
bridge between the event device and the crypto device.

The packet flow from crypto device to the event device can be accomplished
using SW and HW based transfer mechanism.
The Adapter queries an eventdev PMD to determine which mechanism to be used.
The adapter uses an EAL service core function for SW based packet transfer
and uses the eventdev PMD functions to configure HW based packet transfer
between the crypto device and the event device. The crypto adapter uses a new
event type called ``RTE_EVENT_TYPE_CRYPTODEV`` to indicate the event source.

The application can choose to submit a crypto operation directly to
crypto device or send it to the crypto adapter via eventdev based on
RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability.
The first mode is known as the event new(RTE_EVENT_CRYPTO_ADAPTER_OP_NEW)
mode and the second as the event forward(RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD)
mode. The choice of mode can be specified while creating the adapter.
In the former mode, it is an application responsibility to enable ingress
packet ordering. In the latter mode, it is the adapter responsibility to
enable the ingress packet ordering.


Adapter Mode
------------

RTE_EVENT_CRYPTO_ADAPTER_OP_NEW mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the RTE_EVENT_CRYPTO_ADAPTER_OP_NEW mode, application submits crypto
operations directly to crypto device. The adapter then dequeues crypto
completions from crypto device and enqueues them as events to the event device.
This mode does not ensure ingress ordering, if the application directly
enqueues to the cryptodev without going through crypto/atomic stage.
In this mode, events dequeued from the adapter will be treated as new events.
The application needs to specify event information (response information)
which is needed to enqueue an event after the crypto operation is completed.

.. _figure_event_crypto_adapter_op_new:

.. figure:: img/event_crypto_adapter_op_new.*

   Working model of ``RTE_EVENT_CRYPTO_ADAPTER_OP_NEW`` mode


RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode, if HW supports
RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability the application
can directly submit the crypto operations to the cryptodev.
If not, application retrieves crypto adapter's event port using
rte_event_crypto_adapter_event_port_get() API. Then, links its event
queue to this port and starts enqueuing crypto operations as events
to the eventdev. The adapter then dequeues the events and submits the
crypto operations to the cryptodev. After the crypto completions, the
adapter enqueues events to the event device.
Application can use this mode, when ingress packet ordering is needed.
In this mode, events dequeued from the adapter will be treated as
forwarded events. The application needs to specify the cryptodev ID
and queue pair ID (request information) needed to enqueue a crypto
operation in addition to the event information (response information)
needed to enqueue an event after the crypto operation has completed.

.. _figure_event_crypto_adapter_op_forward:

.. figure:: img/event_crypto_adapter_op_forward.*

   Working model of ``RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD`` mode


API Overview
------------

This section has a brief introduction to the event crypto adapter APIs.
The application is expected to create an adapter which is associated with
a single eventdev, then add cryptodev and queue pair to the adapter instance.

Create an adapter instance
~~~~~~~~~~~~~~~~~~~~~~~~~~

An adapter instance is created using ``rte_event_crypto_adapter_create()``. This
function is called with event device to be associated with the adapter and port
configuration for the adapter to setup an event port(if the adapter needs to use
a service function).

Adapter can be started in ``RTE_EVENT_CRYPTO_ADAPTER_OP_NEW`` or
``RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD`` mode.

.. code-block:: c

        int err;
        uint8_t dev_id, id;
        struct rte_event_dev_info dev_info;
        struct rte_event_port_conf conf;
        enum rte_event_crypto_adapter_mode mode;

        err = rte_event_dev_info_get(id, &dev_info);

        conf.new_event_threshold = dev_info.max_num_events;
        conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
        conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;
	mode = RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD;
        err = rte_event_crypto_adapter_create(id, dev_id, &conf, mode);

If the application desires to have finer control of eventdev port allocation
and setup, it can use the ``rte_event_crypto_adapter_create_ext()`` function.
The ``rte_event_crypto_adapter_create_ext()`` function is passed as a callback
function. The callback function is invoked if the adapter needs to use a
service function and needs to create an event port for it. The callback is
expected to fill the ``struct rte_event_crypto_adapter_conf`` structure
passed to it.

For RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode, the event port created by adapter
can be retrieved using ``rte_event_crypto_adapter_event_port_get()`` API.
Application can use this event port to link with event queue on which it
enqueues events towards the crypto adapter.

.. code-block:: c

        uint8_t id, evdev, crypto_ev_port_id, app_qid;
        struct rte_event ev;
        int ret;

        ret = rte_event_crypto_adapter_event_port_get(id, &crypto_ev_port_id);
        ret = rte_event_queue_setup(evdev, app_qid, NULL);
        ret = rte_event_port_link(evdev, crypto_ev_port_id, &app_qid, NULL, 1);

        // Fill in event info and update event_ptr with rte_crypto_op
        memset(&ev, 0, sizeof(ev));
        ev.queue_id = app_qid;
        .
        .
        ev.event_ptr = op;
        ret = rte_event_enqueue_burst(evdev, app_ev_port_id, ev, nb_events);

Querying adapter capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_event_crypto_adapter_caps_get()`` function allows
the application to query the adapter capabilities for an eventdev and cryptodev
combination. This API provides whether cryptodev and eventdev are connected using
internal HW port or not.

.. code-block:: c

        rte_event_crypto_adapter_caps_get(dev_id, cdev_id, &cap);

Adding queue pair to the adapter instance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Cryptodev device id and queue pair are created using cryptodev APIs.
For more information see :doc:`here  <cryptodev_lib>`.

.. code-block:: c

        struct rte_cryptodev_config conf;
        struct rte_cryptodev_qp_conf qp_conf;
        uint8_t cdev_id = 0;
        uint16_t qp_id = 0;

        rte_cryptodev_configure(cdev_id, &conf);
        rte_cryptodev_queue_pair_setup(cdev_id, qp_id, &qp_conf);

These cryptodev id and queue pair are added to the instance using the
``rte_event_crypto_adapter_queue_pair_add()`` API.
The same is removed using ``rte_event_crypto_adapter_queue_pair_del()`` API.
If HW supports RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND
capability, event information must be passed to the add API.

.. code-block:: c

        uint32_t cap;
        int ret;

        ret = rte_event_crypto_adapter_caps_get(id, evdev, &cap);
        if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
                struct rte_event event;

                // Fill in event information & pass it to add API
                rte_event_crypto_adapter_queue_pair_add(id, cdev_id, qp_id, &event);
        } else
                rte_event_crypto_adapter_queue_pair_add(id, cdev_id, qp_id, NULL);

Configure the service function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the adapter uses a service function, the application is required to assign
a service core to the service function as show below.

.. code-block:: c

        uint32_t service_id;

        if (rte_event_crypto_adapter_service_id_get(id, &service_id) == 0)
                rte_service_map_lcore_set(service_id, CORE_ID);

Set event request/response information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode, the application needs
to specify the cryptodev ID and queue pair ID (request information) in
addition to the event information (response information) needed to enqueue
an event after the crypto operation has completed. The request and response
information are specified in the ``struct rte_crypto_op`` private data or
session's private data.

In the RTE_EVENT_CRYPTO_ADAPTER_OP_NEW mode, the application is required
to provide only the response information.

The SW adapter or HW PMD uses ``rte_crypto_op::sess_type`` to
decide whether request/response data is located in the crypto session/
crypto security session or at an offset in the ``struct rte_crypto_op``.
The ``rte_crypto_op::private_data_offset`` is used to locate the request/
response in the ``rte_crypto_op``.

For crypto session, ``rte_cryptodev_sym_session_set_user_data()`` API
will be used to set request/response data. The same data will be obtained
by ``rte_cryptodev_sym_session_get_user_data()`` API.  The
RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA capability indicates
whether HW or SW supports this feature.

For security session, ``rte_security_session_set_private_data()`` API
will be used to set request/response data. The same data will be obtained
by ``rte_security_session_get_private_data()`` API.

For session-less it is mandatory to place the request/response data with
the ``rte_crypto_op``.

.. code-block:: c

        union rte_event_crypto_metadata m_data;
        struct rte_event ev;
        struct rte_crypto_op *op;

        /* Allocate & fill op structure */
        op = rte_crypto_op_alloc();

        memset(&m_data, 0, sizeof(m_data));
        memset(&ev, 0, sizeof(ev));
        /* Fill event information and update event_ptr to rte_crypto_op */
        ev.event_ptr = op;

        if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
                /* Copy response information */
                rte_memcpy(&m_data.response_info, &ev, sizeof(ev));
                /* Copy request information */
                m_data.request_info.cdev_id = cdev_id;
                m_data.request_info.queue_pair_id = qp_id;
                /* Call set API to store private data information */
                rte_cryptodev_sym_session_set_user_data(
                        op->sym->session,
                        &m_data,
                        sizeof(m_data));
        } if (op->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
                uint32_t len = IV_OFFSET + MAXIMUM_IV_LENGTH +
                               (sizeof(struct rte_crypto_sym_xform) * 2);
                op->private_data_offset = len;
                /* Copy response information */
                rte_memcpy(&m_data.response_info, &ev, sizeof(ev));
                /* Copy request information */
                m_data.request_info.cdev_id = cdev_id;
                m_data.request_info.queue_pair_id = qp_id;
                /* Store private data information along with rte_crypto_op */
                rte_memcpy(op + len, &m_data, sizeof(m_data));
        }

Start the adapter instance
~~~~~~~~~~~~~~~~~~~~~~~~~~

The application calls ``rte_event_crypto_adapter_start()`` to start the adapter.
This function calls the start callbacks of the eventdev PMDs for hardware based
eventdev-cryptodev connections and ``rte_service_run_state_set()`` to enable the
service function if one exists.

.. code-block:: c

        rte_event_crypto_adapter_start(id, mode);

.. Note::

         The eventdev to which the event_crypto_adapter is connected needs to
         be started before calling rte_event_crypto_adapter_start().

Get adapter statistics
~~~~~~~~~~~~~~~~~~~~~~

The  ``rte_event_crypto_adapter_stats_get()`` function reports counters defined
in struct ``rte_event_crypto_adapter_stats``. The received packet and
enqueued event counts are a sum of the counts from the eventdev PMD callbacks
if the callback is supported, and the counts maintained by the service function,
if one exists.
