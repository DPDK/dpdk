..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2020 Microsoft Corporation

dpdk-hugepages Application
==========================

The ``dpdk-hugepages`` tool is a Data Plane Development Kit (DPDK) utility
that helps in reserving hugepages.
As well as checking for current settings.


Running the Application
-----------------------

The tool has a number of command line options:

.. code-block:: console

   dpdk-hugepages [options]


Options
-------

* ``-h, --help``

    Display usage information and quit

* ``-s, --show``

    Print the current huge page configuration

* ``-c driver, --clear``

    Clear existing huge page reservation

* ``-m, --mount``

    Mount the huge page filesystem

* ``-u, --unmount``

    Unmount the huge page filesystem

* ``-n NODE, --node=NODE``

    Set NUMA node to reserve pages on

* ``-p SIZE, --pagesize=SIZE``

    Select hugepage size to use.
	If not specified the default system huge page size is used.

* ``-r SIZE, --reserve=SIZE``

    Reserve huge pages.
	Size is in bytes with K, M or G suffix.

* ``--setup SIZE``

    Short cut to clear, unmount, reserve and mount.

.. warning::

   While any user can run the ``dpdk-hugepages.py`` script to view the
   status of huge pages, modifying the setup requires root privileges.


Examples
--------

To display current huge page settings::

   dpdk-hugepages.py -s

To a complete setup of with 2 Gigabyte of 1G huge pages::

   dpdk-hugepages.py -p 1G --setup 2G
