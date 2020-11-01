..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

Driver for the Intel® Dynamic Load Balancer (DLB2)
==================================================

The DPDK dlb poll mode driver supports the Intel® Dynamic Load Balancer.

Prerequisites
-------------

Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
the basic DPDK environment.

Configuration
-------------

The DLB2 PF PMD is a user-space PMD that uses VFIO to gain direct
device access. To use this operation mode, the PCIe PF device must be bound
to a DPDK-compatible VFIO driver, such as vfio-pci.

Eventdev API Notes
------------------

The DLB2 provides the functions of a DPDK event device; specifically, it
supports atomic, ordered, and parallel scheduling events from queues to ports.
However, the DLB2 hardware is not a perfect match to the eventdev API. Some DLB2
features are abstracted by the PMD such as directed ports.

In general the dlb PMD is designed for ease-of-use and does not require a
detailed understanding of the hardware, but these details are important when
writing high-performance code. This section describes the places where the
eventdev API and DLB2 misalign.

Flow ID
~~~~~~~

The flow ID field is preserved in the event when it is scheduled in the
DLB2.

