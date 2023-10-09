.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2022 Intel Corporation.

.. include:: <isonum.txt>

CPFL Poll Mode Driver
=====================

The [*EXPERIMENTAL*] cpfl PMD (**librte_net_cpfl**) provides poll mode driver support
for Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100.
Please refer to
https://www.intel.com/content/www/us/en/products/network-io/infrastructure-processing-units/asic/e2000-asic.html
for more information.

Linux Prerequisites
-------------------

Follow the DPDK :doc:`../linux_gsg/index` to setup the basic DPDK environment.

To get better performance on Intel platforms,
please follow the :doc:`../linux_gsg/nic_perf_intel_platform`.


Recommended Matching List
-------------------------

It is highly recommended to upgrade the MEV-ts release
to avoid compatibility issues with the cpfl PMD.
Here is the suggested matching list which has been tested and verified.

   +------------+------------------+
   |     DPDK   |  MEV-ts release  |
   +============+==================+
   |    23.07   |      0.9.1       |
   +------------+------------------+


Configuration
-------------

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

- ``vport`` (default ``0``)

  The PMD supports creation of multiple vports for one PCI device,
  each vport corresponds to a single ethdev.
  The user can specify the vports with specific ID to be created, and ID should
  be 0 ~ 7 currently, for example:

    -a ca:00.0,vport=[0,2,3]

  Then the PMD will create 3 vports (ethdevs) for device ``ca:00.0``.

  If the parameter is not provided, the vport 0 will be created by default.

- ``rx_single`` (default ``0``)

  There are two queue modes supported by Intel\ |reg| IPU Ethernet E2100 Series,
  single queue mode and split queue mode for Rx queue.

  For the single queue model, the descriptor queue is used by SW to post buffer
  descriptors to HW, and it's also used by HW to post completed descriptors to SW.

  For the split queue model, "RX buffer queues" are used to pass descriptor buffers
  from SW to HW, while RX queues are used only to pass the descriptor completions
  from HW to SW.

  User can choose Rx queue mode, example:

    -a ca:00.0,rx_single=1

  Then the PMD will configure Rx queue with single queue mode.
  Otherwise, split queue mode is chosen by default.

- ``tx_single`` (default ``0``)

  There are two queue modes supported by Intel\ |reg| IPU Ethernet E2100 Series,
  single queue mode and split queue mode for Tx queue.

  For the single queue model, the descriptor queue is used by SW to post buffer
  descriptors to HW, and it's also used by HW to post completed descriptors to SW.

  For the split queue model, "TX completion queues" are used to pass descriptor buffers
  from SW to HW, while TX queues are used only to pass the descriptor completions from
  HW to SW.

  User can choose Tx queue mode, example::

    -a ca:00.0,tx_single=1

  Then the PMD will configure Tx queue with single queue mode.
  Otherwise, split queue mode is chosen by default.

- ``representor`` (default ``not enabled``)

  The cpfl PMD supports the creation of APF/CPF/VF port representors.
  Each port representor corresponds to a single function of that device.
  Using the ``devargs`` option ``representor`` the user can specify
  which functions to create port representors.

  Format is::

    [[c<controller_id>]pf<pf_id>]vf<vf_id>

  Controller_id 0 is host (default), while 1 is accelerator core.
  Pf_id 0 is APF (default), while 1 is CPF.
  Default value can be omitted.

  Create 4 representors for 4 vfs on host APF::

    -a BDF,representor=c0pf0vf[0-3]

  Or::

    -a BDF,representor=pf0vf[0-3]

  Or::

    -a BDF,representor=vf[0-3]

  Create a representor for CPF on accelerator core::

    -a BDF,representor=c1pf1

  Multiple representor devargs are supported. Create 4 representors for 4
  vfs on host APF and one representor for CPF on accelerator core::

    -a BDF,representor=vf[0-3],representor=c1pf1

- ``flow_parser`` (default ``not enabled``)

  The cpfl PMD supports utilizing a JSON config file to translate rte_flow tokens into
  low-level hardware resources.

  The JSON configuration file is provided by the hardware vendor and is intended to work
  exclusively with a specific P4 pipeline configuration, which must be compiled and programmed
  into the hardware.

  The format of the JSON file strictly follows the internal specifications of the hardware
  vendor and is not meant to be modified directly by users.

  Using the ``devargs`` option ``flow_parser`` the user can specify the path
  of a json file, for example::

    -a ca:00.0,flow_parser="refpkg.json"

  Then the PMD will load json file for device ``ca:00.0``.
  The parameter is optional.

Driver compilation and testing
------------------------------

Refer to the document :doc:`build_and_test` for details.

The jansson library must be installed to use rte_flow.

Features
--------

Vector PMD
~~~~~~~~~~

Vector path for Rx and Tx path are selected automatically.
The paths are chosen based on 2 conditions:

- ``CPU``

  On the x86 platform, the driver checks if the CPU supports AVX512.
  If the CPU supports AVX512 and EAL argument ``--force-max-simd-bitwidth``
  is set to 512, AVX512 paths will be chosen.

- ``Offload features``

  The supported HW offload features are described in the document cpfl.ini,
  A value "P" means the offload feature is not supported by vector path.
  If any not supported features are used, cpfl vector PMD is disabled
  and the scalar paths are chosen.


Hairpin queue
~~~~~~~~~~~~~

E2100 Series can loopback packets from RX port to TX port.
This feature is called port-to-port or hairpin.
Currently, the PMD only supports single port hairpin.

Flow offload
~~~~~~~~~~~~

PMD uses a json file to direct CPF PMD to parse rte_flow tokens into
low level hardware resources.

- Required Libraries

  * jansson

    * For Ubuntu, it can be installed using `apt install libjansson-dev`

- run testpmd with the json file, create two vports

   .. code-block:: console

      dpdk-testpmd -c 0x3 -n 4 -a 0000:af:00.6,vport=[0-1],flow_parser="refpkg.json" -- -i

#. Create one flow to forward ETH-IPV4-TCP from I/O port to a local(CPF's) vport. Flow should be created on
   vport X. Group M should match fxp module. Action port_representor Y means forward packet to local vport Y::

   .. code-block:: console

      flow create X ingress group M pattern eth dst is 00:01:00:00:03:14 / ipv4 src is 192.168.0.1 \
      dst is 192.168.0.2 / tcp / end actions port_representor port_id Y / end

#. Send a matched packet, and it should be displayed on PMD::

   .. code-block:: console

      sendp(Ether(dst='00:01:00:00:03:14')/IP(src='192.168.0.1',dst='192.168.0.2')/TCP(),iface="ens25f0")
