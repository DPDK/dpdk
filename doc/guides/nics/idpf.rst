.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2022 Intel Corporation.

.. include:: <isonum.txt>

IDPF Poll Mode Driver
=====================

The [*EXPERIMENTAL*] idpf PMD (**librte_net_idpf**) provides poll mode driver support
for Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100.


Linux Prerequisites
-------------------

Follow the DPDK :doc:`../linux_gsg/index` to setup the basic DPDK environment.

To get better performance on Intel platforms,
please follow the :doc:`../linux_gsg/nic_perf_intel_platform`.


Recommended Matching List
-------------------------

It is highly recommended to upgrade the idpf kernel driver, MEV-ts release
to avoid compatibility issues with the idpf PMD.
Here is the suggested matching list which has been tested and verified.

   +------------+---------------+------------------+
   |    DPDK    | Kernel Driver |  MEV-ts release  |
   +============+===============+==================+
   |    23.07   |    0.0.710    |      0.9.1       |
   +------------+---------------+------------------+


Configuration
-------------

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

- ``vport`` (default ``0``)

  The PMD supports creation of multiple vports for one PCI device,
  each vport corresponds to a single ethdev.
  The user can specify the vports with specific ID to be created, for example::

    -a ca:00.0,vport=[0,2,3]

  Then the PMD will create 3 vports (ethdevs) for device ``ca:00.0``.

  If the parameter is not provided, the vport 0 will be created by default.

- ``rx_single`` (default ``0``)

  There are two queue modes supported by Intel\ |reg| IPU Ethernet ES2000 Series,
  single queue mode and split queue mode for Rx queue.
  User can choose Rx queue mode, example::

    -a ca:00.0,rx_single=1

  Then the PMD will configure Rx queue with single queue mode.
  Otherwise, split queue mode is chosen by default.

- ``tx_single`` (default ``0``)

  There are two queue modes supported by Intel\ |reg| IPU Ethernet ES2000 Series,
  single queue mode and split queue mode for Tx queue.
  User can choose Tx queue mode, example::

    -a ca:00.0,tx_single=1

  Then the PMD will configure Tx queue with single queue mode.
  Otherwise, split queue mode is chosen by default.


Driver compilation and testing
------------------------------

Refer to the document :doc:`build_and_test` for details.


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

  The supported HW offload features are described in the document idpf.ini,
  A value "P" means the offload feature is not supported by vector path.
  If any not supported features are used, idpf vector PMD is disabled
  and the scalar paths are chosen.
