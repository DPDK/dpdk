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

