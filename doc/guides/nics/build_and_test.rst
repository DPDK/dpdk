..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Cavium, Inc

.. _pmd_build_and_test:

Compiling and testing a PMD for a NIC
=====================================

This section demonstrates how to compile and run a Poll Mode Driver (PMD) for
the available Network Interface Cards in DPDK using TestPMD.

TestPMD is one of the reference applications distributed with the DPDK. Its main
purpose is to forward packets between Ethernet ports on a network interface and
as such is the best way to test a PMD.

Refer to the :ref:`testpmd application user guide <testpmd_ug>` for detailed
information on how to build and run testpmd.

Driver Compilation
------------------

To compile a PMD for a platform, build DPDK
as described in the "Getting Started Guide" for your platform.
This will also build testpmd.

Detailed instructions are available
in the :doc:`meson build guide <../prog_guide/build-sdk-meson>`.

The ethdev layer supports below build options for debug purpose:

- ``RTE_ETHDEV_DEBUG_RX`` (default **disabled**)

  Build with debug code on Rx path.

- ``RTE_ETHDEV_DEBUG_TX`` (default **disabled**)

  Build with debug code on Tx path.

- ``RTE_FLOW_DEBUG`` (default **disabled**; enabled automatically on debug builds)

  Build with debug code in asynchronous flow API.

.. Note::

   The ethdev library uses above options to wrap debug code to trace invalid parameters
   on data path APIs, so performance downgrade is expected when enabling those options.
   Each PMD can decide to reuse them to wrap their own debug code in the Rx/Tx path
   and in asynchronous flow API implementation.

Running testpmd in Linux
------------------------

This section demonstrates how to setup and run ``testpmd`` in Linux.

#. Mount huge pages:

   .. code-block:: console

      mkdir /mnt/huge
      mount -t hugetlbfs nodev /mnt/huge

#. Request huge pages:

   Hugepage memory should be reserved as per application requirement. Check
   hugepage size configured in the system and calculate the number of pages
   required.

   To reserve 1024 pages of 2MB:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   .. note::

      Check ``/proc/meminfo`` to find system hugepage size:

      .. code-block:: console

         grep "Hugepagesize:" /proc/meminfo

      Example output:

      .. code-block:: console

         Hugepagesize:       2048 kB

   Mount and request above can be achieved simply with this tool:

   .. code-block:: console

      dpdk-hugepages.py --setup 2G

#. Load ``igb_uio`` or ``vfio-pci`` driver:

   .. code-block:: console

      modprobe uio
      insmod igb_uio.ko

   or

   .. code-block:: console

      modprobe vfio-pci

#. Setup VFIO permissions for regular users before binding to ``vfio-pci``:

   .. code-block:: console

      sudo chmod a+x /dev/vfio

      sudo chmod 0666 /dev/vfio/*

#. Bind the adapters to ``igb_uio`` or ``vfio-pci`` loaded in the previous step:

   .. code-block:: console

      ./usertools/dpdk-devbind.py --bind igb_uio DEVICE1 DEVICE2 ...

   Or setup VFIO permissions for regular users and then bind to ``vfio-pci``:

   .. code-block:: console

      ./usertools/dpdk-devbind.py --bind vfio-pci DEVICE1 DEVICE2 ...

   .. note::

      DEVICE1, DEVICE2 are specified via PCI "domain:bus:slot.func" syntax or
      "bus:slot.func" syntax.

#. Start ``testpmd`` with basic parameters:

   .. code-block:: console

      ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 -- -i

   Successful execution will show initialization messages from EAL, PMD and
   testpmd application. A prompt will be displayed at the end for user commands
   as interactive mode (``-i``) is on.

   .. code-block:: console

      testpmd>

   Refer to the :ref:`testpmd runtime functions <testpmd_runtime>` for a list
   of available commands.

   .. note::
      When ``testpmd`` is built with shared library, use option ``-d`` to load
      the dynamic PMD for ``rte_eal_init``.
