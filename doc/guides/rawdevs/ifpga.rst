..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2022 Intel Corporation.

IFPGA Rawdev Driver
======================

FPGA is used more and more widely in Cloud and NFV, one primary reason is
that FPGA not only provides ASIC performance but also it's more flexible
than ASIC.

FPGA uses Partial Reconfigure (PR) Parts of Bit Stream to achieve its
flexibility. That means one FPGA Device Bit Stream is divided into many Parts
of Bit Stream(each Part of Bit Stream is defined as AFU-Accelerated Function
Unit), and each AFU is a hardware acceleration unit which can be dynamically
reloaded respectively.

By PR (Partial Reconfiguration) AFUs, one FPGA resources can be time-shared by
different users. FPGA hot upgrade and fault tolerance can be provided easily.

The SW IFPGA Rawdev Driver (**ifpga_rawdev**) provides a Rawdev driver
that utilizes Intel FPGA Software Stack OPAE(Open Programmable Acceleration
Engine) for FPGA management.

Implementation details
----------------------

Each instance of IFPGA Rawdev Driver is probed by Intel FpgaDev. In coordination
with OPAE share code IFPGA Rawdev Driver provides common FPGA management ops
for FPGA operation, OPAE provides all following operations:
- FPGA PR (Partial Reconfiguration) management
- FPGA AFUs Identifying
- FPGA Thermal Management
- FPGA Power Management
- FPGA Performance reporting
- FPGA Remote Debug

All configuration parameters are taken by vdev_ifpga_cfg driver. Besides
configuration, vdev_ifpga_cfg driver also hot plugs in IFPGA Bus.

All of the AFUs of one FPGA may share same PCI BDF and AFUs scan depend on
IFPGA Rawdev Driver so IFPGA Bus takes AFU device scan and AFU drivers probe.
All AFU device driver bind to AFU device by its UUID (Universally Unique
Identifier).

To avoid unnecessary code duplication and ensure maximum performance,
handling of AFU devices is left to different PMDs; all the design as
summarized by the following block diagram::

     +---------------------------------------------------------------+
     |                       Application(s)                          |
     +----------------------------.----------------------------------+
                                  |
                                  |
     +----------------------------'----------------------------------+
     |                    DPDK Framework (APIs)                      |
     +----------|------------|--------.---------------------|--------+
               /              \                             |
              /                \                            |
     +-------'-------+  +-------'-------+          +--------'--------+
     |    Eth PMD    |  |   Crypto PMD  |          |                 |
     +-------.-------+  +-------.-------+          |                 |
             |                  |                  |                 |
             |                  |                  |                 |
     +-------'-------+  +-------'-------+          |      IFPGA      |
     |  Eth AFU Dev  |  |Crypto AFU Dev |          |  Rawdev Driver  |
     +-------.-------+  +-------.-------+          |(OPAE Share Code)|
             |                  |                  |                 |
             |                  |          Rawdev  |                 |
     +-------'------------------'-------+    Ops   |                 |
     |              IFPGA Bus           | -------->|                 |
     +-----------------.----------------+          +--------.--------+
                       |                                    |
         Hot-plugin -->|                                    |
                       |                                    |
     +-----------------'------------------+        +--------'--------+
     |        vdev_ifpga_cfg driver       |        |  Intel FpgaDev  |
     +------------------------------------+        +-----------------+


Run-time parameters
-------------------

This driver is invoked automatically in systems added with Intel FPGA,
but PR and IFPGA Bus scan is triggered by command line using
``--vdev 'ifpga_rawdev_cfg`` EAL option.

The following device parameters are supported:

- ``ifpga`` [string]

  Provide a specific Intel FPGA device PCI BDF. Can be provided multiple
  times for additional instances.

- ``port`` [int]

  Each FPGA can provide many channels to PR AFU by software, each channels
  is identified by this parameter.

- ``afu_bts`` [string]

  If null, the AFU Bit Stream has been PR in FPGA, if not forces PR and
  identifies AFU Bit Stream file.


Open FPGA Stack
=====================

Open FPGA Stack (OFS) is a collection of RTL and open source software providing
interfaces to access the instantiated RTL easily in an FPGA. OFS leverages the
DFL for the implementation of the FPGA RTL design.

OFS designs allow for the arrangement of software interfaces across multiple
PCIe endpoints. Some of these interfaces may be PFs defined in the static region
that connect to interfaces in an IP that is loaded via Partial Reconfiguration (PR).
And some of these interfaces may be VFs defined in the PR region that can be
reconfigured by the end-user. Furthermore, these PFs/VFs may use DFLs such that
features may be discovered and accessed in user space with the aid of a generic
kernel driver like vfio-pci. The diagram below depicts an example design with one
PF and two VFs. In this example, it will export the management functions via PF0
and acceleration functions via VF0 and VF1, leverage VFIO to export the MMIO space
to an application.::

     +-----------------+  +-------------+  +------------+
     | FPGA Management |  |  DPDK App   |  |  User App  |
     |      App        |  |             |  |            |
     +--------+--------+  +------+------+  +-----+------+
              |                  |               |
     +--------+--------+  +------+------+        |
     |    IFPGA PMD    |  |   AFU PMD   |        |
     +--------+--------+  +------+------+        |
              |                  |               |
     +--------+------------------+---------------+------+
     |                VFIO-PCI                          |
     +--------+------------------+---------------+------+
              |                  |               |
     +--------+--------+  +------+------+  +-----+------+
     |       PF0       |  |   PF0_VF0   |  |  PF0_VF1   |
     +-----------------+  +-------------+  +------------+

As accelerators are specialized hardware, they are typically limited in the
number installed in a given system. Many use cases require them to be shared
across multiple software contexts or threads of software execution, either
through partitioning of individual dedicated resources, or virtualization of
shared resources. OFS provides several models to share the AFU resources via
PR mechanism and hardware-based virtualization schemes.

1. Legacy model.
   With legacy model FPGA cards like Intel PAC N3000 or N5000, there is
   a notion that the boundary between the AFU and the shell is also the unit of
   PR for those FPGA platforms. This model is only able to handle a
   single context, because it only has one PR engine, and one PR region which
   has an associated Port device.
2. Multiple VFs per PR slot.
   In this model, available AFU resources may allow instantiation of many VFs
   which have a dedicated PCIe function with their own dedicated MMIO space, or
   partition a region of MMIO space on a single PCIe function. Intel PAC N6000
   card has implemented this model.
   In this model, the AFU/PR slot was not connected to port device. For DFL's view,
   the Next_AFU pointer in FIU feature header of port device points to NULL in this
   model. On the other hand, each VF can start with an AFU feature header without
   being connected to a FIU Port feature header.

The VFs are created through the Linux kernel driver before we use them in DPDK.

OFS provides the diversity for accessing the AFU resource to RTL developer.
An IP designer may choose to add more than one PF for interfacing with IP
on the FPGA and choose different model to access the AFU resource.

There is one reference architecture design using the "Multiple VFs per PR slot"
model for OFS as illustrated below. In this reference design, it exports the
FPGA management functions via PF0. PF1 will bind with DPDK virtio driver
presenting itself as a network interface to the application. PF2 will bind to the
vfio-pci driver allowing the user space software to discover and interface
with the specific workload like diagnostic test. It leverages AFU PMD driver to
access the AFU resources in DPDK.::

                              +----------------------+
                              |   PF/VF mux/demux    |
                              +--+--+-----+------+-+-+
                                 |  |     |      | |
        +------------------------+  |     |      | |
  PF0   |                 +---------+   +-+      | |
    +---+---+             |         +---+----+   | |
    |  DFH  |             |         |   DFH  |   | |
    +-------+       +-----+----+    +--------+   | |
    |  FME  |       |  VirtIO  |    |  Test  |   | |
    +---+---+       +----------+    +--------+   | |
        |                PF1            PF2      | |
        |                                        | |
        |                             +----------+ |
        |                             |           ++
        |                             |           |
        |                             | PF0_VF0   | PF0_VF1
        |           +-----------------+-----------+------------+
        |           |           +-----+-----------+--------+   |
        |           |           |     |           |        |   |
        |           | +------+  |  +--+ -+     +--+---+    |   |
        |           | | Port |  |  | DFH |     |  DFH |    |   |
        +-----------+ +------+  |  +-----+     +------+    |   |
                    |           |  | DEV |     |  DEV |    |   |
                    |           |  +-----+     +------+    |   |
                    |           |            PR Slot       |   |
                    |           +--------------------------+   |
                    | Port Gasket                              |
                    +------------------------------------------+
