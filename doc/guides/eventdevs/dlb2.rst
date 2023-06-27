..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

Driver for the Intel® Dynamic Load Balancer (DLB)
=================================================

The DPDK DLB poll mode driver supports the Intel® Dynamic Load Balancer,
hardware versions 2.0 and 2.5.

Prerequisites
-------------

Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
the basic DPDK environment.

Configuration
-------------

The DLB PF PMD is a user-space PMD that uses VFIO to gain direct
device access. To use this operation mode, the PCIe PF device must
be bound to a DPDK-compatible VFIO driver, such as vfio-pci.

Eventdev API Notes
------------------

The DLB PMD provides the functions of a DPDK event device; specifically, it
supports atomic, ordered, and parallel scheduling events from queues to ports.
However, the DLB hardware is not a perfect match to the eventdev API. Some DLB
features are abstracted by the PMD such as directed ports.

In general the DLB PMD is designed for ease-of-use and does not require a
detailed understanding of the hardware, but these details are important when
writing high-performance code. This section describes the places where the
eventdev API and DLB misalign.

Scheduling Domain Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

DLB supports 32 scheduling domains.
When one is configured, it allocates load-balanced and
directed queues, ports, credits, and other hardware resources. Some
resource allocations are user-controlled -- the number of queues, for example
-- and others, like credit pools (one directed and one load-balanced pool per
scheduling domain), are not.

The DLB is a closed system eventdev, and as such the ``nb_events_limit`` device
setup argument and the per-port ``new_event_threshold`` argument apply as
defined in the eventdev header file. The limit is applied to all enqueues,
regardless of whether it will consume a directed or load-balanced credit.

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
queue's reorder buffer size.  DLB has 2 groups of ordered queues, where each
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
traffic, the DLB PMD detects when an event is being sent to a directed queue
and overrides its scheduling type. Note that the originally selected scheduling
type (atomic, ordered, or parallel) is not preserved, and an event's sched_type
will be set to ``RTE_SCHED_TYPE_ATOMIC`` when it is dequeued from a directed
port.

Finally, even though all 3 event types are supported on the same QID by
converting unordered events to ordered, such use should be discouraged as much
as possible, since mixing types on the same queue uses valuable reorder
resources, and orders events which do not require ordering.

Flow ID
~~~~~~~

The flow ID field is preserved in the event when it is scheduled in the
DLB.

Hardware Credits
~~~~~~~~~~~~~~~~

DLB uses a hardware credit scheme to prevent software from overflowing hardware
event storage, with each unit of storage represented by a credit. A port spends
a credit to enqueue an event, and hardware refills the ports with credits as the
events are scheduled to ports. Refills come from credit pools.

For DLB v2.5, there is a single credit pool used for both load balanced and
directed traffic.

For DLB v2.0, each port is a member of both a load-balanced credit pool and a
directed credit pool. The load-balanced credits are used to enqueue to
load-balanced queues, and directed credits are used for directed queues.
These pools' sizes are controlled by the nb_events_limit field in struct
rte_event_dev_config. The load-balanced pool is sized to contain
nb_events_limit credits, and the directed pool is sized to contain
nb_events_limit/2 credits. The directed pool size can be overridden with the
num_dir_credits devargs argument, like so:

    .. code-block:: console

       --allow ea:00.0,num_dir_credits=<value>

This can be used if the default allocation is too low or too high for the
specific application needs. The PMD also supports a devarg that limits the
max_num_events reported by rte_event_dev_info_get():

    .. code-block:: console

       --allow ea:00.0,max_num_events=<value>

By default, max_num_events is reported as the total available load-balanced
credits. If multiple DLB-based applications are being used, it may be desirable
to control how many load-balanced credits each application uses, particularly
when application(s) are written to configure nb_events_limit equal to the
reported max_num_events.

Each port is a member of both credit pools. A port's credit allocation is
defined by its low watermark, high watermark, and refill quanta. These three
parameters are calculated by the DLB PMD like so:

- The load-balanced high watermark is set to the port's enqueue_depth.
  The directed high watermark is set to the minimum of the enqueue_depth and
  the directed pool size divided by the total number of ports.
- The refill quanta is set to half the high watermark.
- The low watermark is set to the minimum of 16 and the refill quanta.

When the eventdev is started, each port is pre-allocated a high watermark's
worth of credits. For example, if an eventdev contains four ports with enqueue
depths of 32 and a load-balanced credit pool size of 4096, each port will start
with 32 load-balanced credits, and there will be 3968 credits available to
replenish the ports. Thus, a single port is not capable of enqueueing up to the
nb_events_limit (without any events being dequeued), since the other ports are
retaining their initial credit allocation; in short, all ports must enqueue in
order to reach the limit.

If a port attempts to enqueue and has no credits available, the enqueue
operation will fail and the application must retry the enqueue. Credits are
replenished asynchronously by the DLB hardware.

Software Credits
~~~~~~~~~~~~~~~~

The DLB is a "closed system" event dev, and the DLB PMD layers a software
credit scheme on top of the hardware credit scheme in order to comply with
the per-port backpressure described in the eventdev API.

The DLB's hardware scheme is local to a queue/pipeline stage: a port spends a
credit when it enqueues to a queue, and credits are later replenished after the
events are dequeued and released.

In the software credit scheme, a credit is consumed when a new (.op =
RTE_EVENT_OP_NEW) event is injected into the system, and the credit is
replenished when the event is released from the system (either explicitly with
RTE_EVENT_OP_RELEASE or implicitly in dequeue_burst()).

In this model, an event is "in the system" from its first enqueue into eventdev
until it is last dequeued. If the event goes through multiple event queues, it
is still considered "in the system" while a worker thread is processing it.

A port will fail to enqueue if the number of events in the system exceeds its
``new_event_threshold`` (specified at port setup time). A port will also fail
to enqueue if it lacks enough hardware credits to enqueue; load-balanced
credits are used to enqueue to a load-balanced queue, and directed credits are
used to enqueue to a directed queue.

The out-of-credit situations are typically transient, and an eventdev
application using the DLB ought to retry its enqueues if they fail.
If enqueue fails, DLB PMD sets rte_errno as follows:

- -ENOSPC: Credit exhaustion (either hardware or software)
- -EINVAL: Invalid argument, such as port ID, queue ID, or sched_type.

Depending on the pipeline the application has constructed, it's possible to
enter a credit deadlock scenario wherein the worker thread lacks the credit
to enqueue an event, and it must dequeue an event before it can recover the
credit. If the worker thread retries its enqueue indefinitely, it will not
make forward progress. Such deadlock is possible if the application has event
"loops", in which an event in dequeued from queue A and later enqueued back to
queue A.

Due to this, workers should stop retrying after a time, release the events it
is attempting to enqueue, and dequeue more events. It is important that the
worker release the events and don't simply set them aside to retry the enqueue
again later, because the port has limited history list size (by default, same
as port's dequeue_depth).

Priority
~~~~~~~~

The DLB supports event priority and per-port queue service priority, as
described in the eventdev header file. The DLB does not support 'global' event
queue priority established at queue creation time.

DLB supports 4 event and queue service priority levels. For both priority types,
the PMD uses the upper three bits of the priority field to determine the DLB
priority, discarding the 5 least significant bits. But least significant bit out
of 3 priority bits is effectively ignored for binning into 4 priorities. The
discarded 5 least significant event priority bits are not preserved when an event
is enqueued.

Note that event priority only works within the same event type.
When atomic and ordered or unordered events are enqueued to same QID, priority
across the types is always equal, and both types are served in a round robin manner.

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

Atomic Inflights Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the last stage prior to scheduling an atomic event to a CQ, DLB holds the
inflight event in a temporary buffer that is divided among load-balanced
queues. If a queue's atomic buffer storage fills up, this can result in
head-of-line-blocking. For example:

- An LDB queue allocated N atomic buffer entries
- All N entries are filled with events from flow X, which is pinned to CQ 0.

Until CQ 0 releases 1+ events, no other atomic flows for that LDB queue can be
scheduled. The likelihood of this case depends on the eventdev configuration,
traffic behavior, event processing latency, potential for a worker to be
interrupted or otherwise delayed, etc.

By default, the PMD allocates 64 buffer entries for each load-balanced queue,
which provides an even division across all 32 queues but potentially wastes
buffer space (e.g. if not all queues are used, or aren't used for atomic
scheduling).

QID Depth Threshold
~~~~~~~~~~~~~~~~~~~

DLB supports setting and tracking queue depth thresholds. Hardware uses
the thresholds to track how full a queue is compared to its threshold.
Four buckets are used

- Less than or equal to 50% of queue depth threshold
- Greater than 50%, but less than or equal to 75% of depth threshold
- Greater than 75%, but less than or equal to 100% of depth threshold
- Greater than 100% of depth thresholds

Per queue threshold metrics are tracked in the DLB xstats, and are also
returned in the impl_opaque field of each received event.

The per qid threshold can be specified as part of the device args, and
can be applied to all queues, a range of queues, or a single queue, as
shown below.

    .. code-block:: console

       --allow ea:00.0,qid_depth_thresh=all:<threshold_value>
       --allow ea:00.0,qid_depth_thresh=qidA-qidB:<threshold_value>
       --allow ea:00.0,qid_depth_thresh=qid:<threshold_value>

Class of service
~~~~~~~~~~~~~~~~

DLB supports provisioning the DLB bandwidth into 4 classes of service.
A LDB port or range of LDB ports may be configured to use one of the classes.
If a port's COS is not defined, then it will be allocated from class 0,
class 1, class 2, or class 3, in that order, depending on availability.

The sum of the cos_bw values may not exceed 100, and no more than
16 LDB ports may be assigned to a given class of service. If port cos is
not defined on the command line, then each class is assigned 25% of the
bandwidth, and the available load balanced ports are split between the classes.
Per-port class of service and bandwidth can be specified in the devargs,
as follows.

    .. code-block:: console

       --allow ea:00.0,port_cos=Px-Py:<0-3>,cos_bw=5:10:80:5
       --allow ea:00.0,port_cos=Px:<0-3>,cos_bw=5:10:80:5

Use X86 Vector Instructions
~~~~~~~~~~~~~~~~~~~~~~~~~~~

DLB supports using x86 vector instructions to optimize the data path.

The default mode of operation is to use scalar instructions, but
the use of vector instructions can be enabled in the devargs, as
follows

    .. code-block:: console

       --allow ea:00.0,vector_opts_enabled=<y/Y>

Maximum CQ Depth
~~~~~~~~~~~~~~~~

DLB supports configuring the maximum depth of a consumer queue (CQ).
The depth must be between 32 and 128, and must be a power of 2. Note
that credit deadlocks may occur as a result of changing the default depth.
To prevent deadlock, the user may also need to configure the maximum
enqueue depth.

    .. code-block:: console

       --allow ea:00.0,max_cq_depth=<depth>

Maximum Enqueue Depth
~~~~~~~~~~~~~~~~~~~~~

DLB supports configuring the maximum enqueue depth of a producer port (PP).
The depth must be between 32 and 1024, and must be a power of 2.

    .. code-block:: console

       --allow ea:00.0,max_enqueue_depth=<depth>

Producer Coremask
~~~~~~~~~~~~~~~~~

For best performance, applications running on certain cores should use
the DLB device locally available on the same tile along with other
resources. To allocate optimal resources, probing is done for each
producer port (PP) for a given CPU and the best performing ports are
allocated to producers. The cpu used for probing is either the first
core of producer coremask (if present) or the second core of EAL
coremask. This will be extended later to probe for all CPUs in the
producer coremask or EAL coremask. Producer coremask can be passed
along with the BDF of the DLB devices.

    .. code-block:: console

       -a xx:y.z,producer_coremask=<core_mask>

Default LDB Port Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~

For optimal load balancing ports that map to one or more QIDs in common
should not be in numerical sequence. The port->QID mapping is application
dependent, but the driver interleaves port IDs as much as possible to
reduce the likelihood of sequential ports mapping to the same QID(s).

Hence, DLB uses an initial allocation of Port IDs to maximize the
average distance between an ID and its immediate neighbors. (i.e.the
distance from 1 to 0 and to 2, the distance from 2 to 1 and to 3, etc.).
Initial port allocation option can be passed through devarg. If y (or Y)
inial port allocation will be used, otherwise initial port allocation
won't be used.

    .. code-block:: console

       --allow ea:00.0,default_port_allocation=<y/Y>

QE Weight
~~~~~~~~~

DLB supports advanced scheduling mechanisms, such as CQ weight.
Each load balanced CQ has a configurable work capacity (max 256)
which corresponds to the total QE weight DLB will allow to be enqueued
to that consumer. Every load balanced event/QE carries a weight of 0, 2, 4,
or 8 and DLB will increment a (per CQ) load indicator when it schedules a
QE to that CQ. The weight is also stored in the history list. When a
completion arrives, the weight is popped from the history list and used to
decrement the load indicator. This creates a new scheduling condition - a CQ
whose load is equal to or in excess of capacity is not available for traffic.
Note that the weight may not exceed the maximum CQ depth.

Example command to enable QE Weight feature:

    .. code-block:: console

       --allow ea:00.0,enable_cq_weight=<y/Y>

Running Eventdev Applications with DLB Device
---------------------------------------------

This section explains how to run eventdev applications
with DLB hardware as well as difference in command line parameter
to switch between a DLB hardware and a virtual eventdev device such as SW0, hence
users can run applications with or without DLB device to compare performance of
a DLB device.

In order to run eventdev applications, DLB device must be bound
to a DPDK-compatible VFIO driver, such as vfio-pci.

Example command to bind DLB device to vfio-pci driver:

    .. code-block:: console

       ../usertools/dpdk-devbind.py -b vfio-pci ea:00.0

Eventdev applications can be run with or without a DLB device.
Below examples give details of running eventdev application without DLB device
and with DLB device. Notice that the primary difference between two examples are
passing the parameter ``--vdev <driver><id>``. The first example run uses a virtual
eventdev device SW0 while second example run directly and picks DLB device from
VFIO driver.

Example command to run eventdev application without a DLB device:

	.. code-block:: console

	   sudo <build_dir>/app/dpdk-test-eventdev --vdev=event_sw0 -- \
					--test=order_queue --plcores 1 --wlcores 2,3

After binding DLB device to a supported pci driver such as vfio-pci,
eventdev applications can be run on the DLB device.

Example command to run eventdev application with a DLB device:

	.. code-block:: console

		sudo build/app/dpdk-test-eventdev -- --test=order_queue\
			--plcores=1 --wlcores=2-7 --stlist=o --worker_deq_depth=128\
			--prod_enq_burst_sz=64 --nb_flows=64 --nb_pkts=1000000

A particular DLB device can also be picked from command line by passing
	``--a`` or  ``--allow`` option:

	.. code-block:: console

		sudo build/app/dpdk-test-eventdev --allow ea:00.0 -- --test=order_queue\
			--plcores=1 --wlcores=2-7 --stlist=o --worker_deq_depth=128\
			--prod_enq_burst_sz=64 --nb_flows=64 --nb_pkts=1000000

Debugging options
~~~~~~~~~~~~~~~~~

To specify log level for a DLB device use ``--log-level=dlb,8``.
Example command to run eventdev application with a DLB device log level enabled:

	.. code-block:: console

		sudo build/app/dpdk-test-eventdev --allow ea:00.0 --log-level=dlb,8 -- --test=order_queue\
			--plcores=1 --wlcores=2-7 --stlist=o --worker_deq_depth=128\
			--prod_enq_burst_sz=64 --nb_flows=64 --nb_pkts=1000000
