..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

Driver for the Intel® Dynamic Load Balancer (DLB)
=================================================

The DPDK dlb poll mode driver supports the Intel® Dynamic Load Balancer.

Prerequisites
-------------

Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
the basic DPDK environment.

Configuration
-------------

The DLB PF PMD is a user-space PMD that uses VFIO to gain direct
device access. To use this operation mode, the PCIe PF device must be bound
to a DPDK-compatible VFIO driver, such as vfio-pci.

Eventdev API Notes
------------------

The DLB provides the functions of a DPDK event device; specifically, it
supports atomic, ordered, and parallel scheduling events from queues to ports.
However, the DLB hardware is not a perfect match to the eventdev API. Some DLB
features are abstracted by the PMD (e.g. directed ports), some are only
accessible as vdev command-line parameters, and certain eventdev features are
not supported (e.g. the event flow ID is not maintained during scheduling).

In general the dlb PMD is designed for ease-of-use and does not require a
detailed understanding of the hardware, but these details are important when
writing high-performance code. This section describes the places where the
eventdev API and DLB misalign.

Scheduling Domain Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are 32 scheduling domainis the DLB.
When one is configured, it allocates load-balanced and
directed queues, ports, credits, and other hardware resources. Some
resource allocations are user-controlled -- the number of queues, for example
-- and others, like credit pools (one directed and one load-balanced pool per
scheduling domain), are not.

The DLB is a closed system eventdev, and as such the ``nb_events_limit`` device
setup argument and the per-port ``new_event_threshold`` argument apply as
defined in the eventdev header file. The limit is applied to all enqueues,
regardless of whether it will consume a directed or load-balanced credit.

Reconfiguration
~~~~~~~~~~~~~~~

The Eventdev API allows one to reconfigure a device, its ports, and its queues
by first stopping the device, calling the configuration function(s), then
restarting the device. The DLB does not support configuring an individual queue
or port without first reconfiguring the entire device, however, so there are
certain reconfiguration sequences that are valid in the eventdev API but not
supported by the PMD.

Specifically, the PMD supports the following configuration sequence:
1. Configure and start the device
2. Stop the device
3. (Optional) Reconfigure the device
4. (Optional) If step 3 is run:

   a. Setup queue(s). The reconfigured queue(s) lose their previous port links.
   b. The reconfigured port(s) lose their previous queue links.

5. (Optional, only if steps 4a and 4b are run) Link port(s) to queue(s)
6. Restart the device. If the device is reconfigured in step 3 but one or more
   of its ports or queues are not, the PMD will apply their previous
   configuration (including port->queue links) at this time.

The PMD does not support the following configuration sequences:
1. Configure and start the device
2. Stop the device
3. Setup queue or setup port
4. Start the device

This sequence is not supported because the event device must be reconfigured
before its ports or queues can be.

Load-Balanced Queues
~~~~~~~~~~~~~~~~~~~~

A load-balanced queue can support atomic and ordered scheduling, or atomic and
unordered scheduling, but not atomic and unordered and ordered scheduling. A
queue's scheduling types are controlled by the event queue configuration.

If the user sets the ``RTE_EVENT_QUEUE_CFG_ALL_TYPES`` flag, the
``nb_atomic_order_sequences`` determines the supported scheduling types.
With non-zero ``nb_atomic_order_sequences``, the queue is configured for atomic
and ordered scheduling. In this case, ``RTE_SCHED_TYPE_PARALLEL`` scheduling is
supported by scheduling those events as ordered events.  Note that when the
event is dequeued, its sched_type will be ``RTE_SCHED_TYPE_ORDERED``. Else if
``nb_atomic_order_sequences`` is zero, the queue is configured for atomic and
unordered scheduling. In this case, ``RTE_SCHED_TYPE_ORDERED`` is unsupported.

If the ``RTE_EVENT_QUEUE_CFG_ALL_TYPES`` flag is not set, schedule_type
dictates the queue's scheduling type.

The ``nb_atomic_order_sequences`` queue configuration field sets the ordered
queue's reorder buffer size.  DLB has 4 groups of ordered queues, where each
group is configured to contain either 1 queue with 1024 reorder entries, 2
queues with 512 reorder entries, and so on down to 32 queues with 32 entries.

When a load-balanced queue is created, the PMD will configure a new sequence
number group on-demand if num_sequence_numbers does not match a pre-existing
group with available reorder buffer entries. If all sequence number groups are
in use, no new group will be created and queue configuration will fail. (Note
that when the PMD is used with a virtual DLB device, it cannot change the
sequence number configuration.)

The queue's ``nb_atomic_flows`` parameter is ignored by the DLB PMD, because
the DLB does not limit the number of flows a queue can track. In the DLB, all
load-balanced queues can use the full 16-bit flow ID range.

Load-balanced and Directed Ports
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

DLB ports come in two flavors: load-balanced and directed. The eventdev API
does not have the same concept, but it has a similar one: ports and queues that
are singly-linked (i.e. linked to a single queue or port, respectively).

The ``rte_event_dev_info_get()`` function reports the number of available
event ports and queues (among other things). For the DLB PMD, max_event_ports
and max_event_queues report the number of available load-balanced ports and
queues, and max_single_link_event_port_queue_pairs reports the number of
available directed ports and queues.

When a scheduling domain is created in ``rte_event_dev_configure()``, the user
specifies ``nb_event_ports`` and ``nb_single_link_event_port_queues``, which
control the total number of ports (load-balanced and directed) and the number
of directed ports. Hence, the number of requested load-balanced ports is
``nb_event_ports - nb_single_link_event_ports``. The ``nb_event_queues`` field
specifies the total number of queues (load-balanced and directed). The number
of directed queues comes from ``nb_single_link_event_port_queues``, since
directed ports and queues come in pairs.

When a port is setup, the ``RTE_EVENT_PORT_CFG_SINGLE_LINK`` flag determines
whether it should be configured as a directed (the flag is set) or a
load-balanced (the flag is unset) port. Similarly, the
``RTE_EVENT_QUEUE_CFG_SINGLE_LINK`` queue configuration flag controls
whether it is a directed or load-balanced queue.

Load-balanced ports can only be linked to load-balanced queues, and directed
ports can only be linked to directed queues. Furthermore, directed ports can
only be linked to a single directed queue (and vice versa), and that link
cannot change after the eventdev is started.

The eventdev API does not have a directed scheduling type. To support directed
traffic, the dlb PMD detects when an event is being sent to a directed queue
and overrides its scheduling type. Note that the originally selected scheduling
type (atomic, ordered, or parallel) is not preserved, and an event's sched_type
will be set to ``RTE_SCHED_TYPE_ATOMIC`` when it is dequeued from a directed
port.

