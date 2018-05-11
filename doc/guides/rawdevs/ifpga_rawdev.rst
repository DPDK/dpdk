..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

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

Build options
-------------

- ``CONFIG_RTE_LIBRTE_IFPGA_BUS`` (default ``y``)

   Toggle compilation of IFPGA Bus library.

- ``CONFIG_RTE_LIBRTE_IFPGA_RAWDEV`` (default ``y``)

   Toggle compilation of the ``ifpga_rawdev`` driver.

Run-time parameters
-------------------

This driver is invoked automatically in systems added with Intel FPGA,
but PR and IFPGA Bus scan is trigged by command line using
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
