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

