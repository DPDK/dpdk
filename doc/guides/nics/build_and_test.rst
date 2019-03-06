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

To compile a PMD for a platform, run make with appropriate target as shown below.
Use "make" command in Linux and "gmake" in FreeBSD. This will also build testpmd.

To check available targets:

.. code-block:: console

   cd <DPDK-source-directory>
   make showconfigs

Example output:

.. code-block:: console

   arm-armv7a-linux-gcc
   arm64-armv8a-linux-gcc
   arm64-dpaa2-linux-gcc
   arm64-thunderx-linux-gcc
   arm64-xgene1-linux-gcc
   i686-native-linux-gcc
   i686-native-linux-icc
   ppc_64-power8-linux-gcc
   x86_64-native-freebsd-clang
   x86_64-native-freebsd-gcc
   x86_64-native-linux-clang
   x86_64-native-linux-gcc
   x86_64-native-linux-icc
   x86_x32-native-linux-gcc

To compile a PMD for Linux x86_64 gcc target, run the following "make" command:

.. code-block:: console

   make install T=x86_64-native-linux-gcc

Use ARM (ThunderX, DPAA, X-Gene) or PowerPC target for respective platform.

For more information, refer to the :ref:`Getting Started Guide for Linux <linux_gsg>`
or :ref:`Getting Started Guide for FreeBSD <freebsd_gsg>` depending on your platform.

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

#. Load ``igb_uio`` or ``vfio-pci`` driver:

   .. code-block:: console

      modprobe uio
      insmod ./x86_64-native-linux-gcc/kmod/igb_uio.ko

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

      ./x86_64-native-linux-gcc/app/testpmd -l 0-3 -n 4 -- -i

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
