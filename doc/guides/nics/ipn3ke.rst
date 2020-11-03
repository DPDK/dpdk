..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

IPN3KE Poll Mode Driver
=======================

The ipn3ke PMD (**librte_net_ipn3ke**) provides poll mode driver support
for Intel® FPGA PAC(Programmable Acceleration Card) N3000 based on
the Intel Ethernet Controller X710/XXV710 and Intel Arria 10 FPGA.

In this card, FPGA is an acceleration bridge between network interface
and the Intel Ethernet Controller. Although both FPGA and Ethernet
Controllers are connected to CPU with PCIe Gen3x16 Switch, all the
packet RX/TX is handled by Intel Ethernet Controller. So from application
point of view the data path is still the legacy Intel Ethernet Controller
X710/XXV710 PMD. Besides this, users can enable more acceleration
features by FPGA IP.

Prerequisites
-------------

- Identifying your adapter using `Intel Support
  <http://www.intel.com/support>`_ and get the latest NVM/FW images.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

- To get better performance on Intel platforms, please follow the "How to get best performance with NICs on Intel platforms"
  section of the :ref:`Getting Started Guide for Linux <linux_gsg>`.


Pre-Installation Configuration
------------------------------


Runtime Config Options
~~~~~~~~~~~~~~~~~~~~~~

- ``AFU name``

  AFU name identifies which AFU is used by IPN3KE. The AFU name format is "Port|BDF",
  Each FPGA can be divided into four blocks at most. "Port" identifies which FPGA block
  the AFU bitstream belongs to, but currently only 0 IPN3KE support. "BDF" means FPGA PCIe BDF.
  For example::

    --vdev 'ipn3ke_cfg0,afu=0|b3:00.0'

- ``FPGA Acceleration list``

  For IPN3KE FPGA can provide different bitstream, different bitstream includes different
  Acceleration, so users need to identify which Acceleration is used. Current IPN3KE can
  support TM and Flow Acceleration, for example::

    --vdev 'ipn3ke_cfg0,afu=0|b3:00.0,fpga_acc={tm|flow}'

- ``I40e PF name list``

  Users need to bind FPGA LineSidePort to FVL PF. So I40e PF name list should be involved in
  startup command. For example::

    --vdev 'ipn3ke_cfg0,afu=0|b3:00.0,fpga_acc={tm|flow},i40e_pf={0000:b1:00.0|0000:b1:00.1|0000:b1:00.2|0000:b1:00.3|0000:b5:00.0|0000:b5:00.1|0000:b5:00.2|0000:b5:00.3}'

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Sample Application Notes
------------------------

Packet TX/RX with FPGA Pass-through image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

FPGA Pass-through bitstream is original FPGA Image.

To start ``testpmd``, and add I40e PF to FPGA network port:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-15 -n 4 --vdev 'ifpga_rawdev_cfg0,ifpga=b3:00.0,port=0' --vdev 'ipn3ke_cfg0,afu=0|b3:00.0,i40e_pf={0000:b1:00.0|0000:b1:00.1|0000:b1:00.2|0000:b1:00.3|0000:b5:00.0|0000:b5:00.1|0000:b5:00.2|0000:b5:00.3}' -- -i --no-numa --port-topology=loop

HQoS and flow acceleration
~~~~~~~~~~~~~~~~~~~~~~~~~~

HQoS and flow acceleration bitstream is used to offloading HQoS and flow classifier.

To start ``testpmd``, and add I40e PF to FPGA network port, enable FPGA HQoS and Flow Acceleration:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-15 -n 4 --vdev 'ifpga_rawdev_cfg0,ifpga=b3:00.0,port=0' --vdev 'ipn3ke_cfg0,afu=0|b3:00.0,fpga_acc={tm|flow},i40e_pf={0000:b1:00.0|0000:b1:00.1|0000:b1:00.2|0000:b1:00.3|0000:b5:00.0|0000:b5:00.1|0000:b5:00.2|0000:b5:00.3}' -- -i --no-numa --forward-mode=macswap

Limitations or Known issues
---------------------------

19.05 limitation
~~~~~~~~~~~~~~~~

Ipn3ke code released in 19.05 is for evaluation only.
