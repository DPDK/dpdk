..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Canonical Limited. All rights reserved.


dpdk-pmdinfo Application
========================

The ``dpdk-pmdinfo.py`` tool is a Data Plane Development Kit (DPDK) utility that
can dump a PMDs hardware support info in the JSON format.

Synopsis
--------

::

   dpdk-pmdinfo.py [-h] [-p] [-v] ELF_FILE [ELF_FILE ...]

Arguments
---------

.. program:: dpdk-pmdinfo.py

.. option:: -h, --help

   Show the inline help.

.. option:: -p, --search-plugins

   In addition of ``ELF_FILE``\s and their linked dynamic libraries,
   also scan the DPDK plugins path.

.. option:: -v, --verbose

   Display warnings due to linked libraries not found
   or ELF/JSON parsing errors in these libraries.
   Use twice to show debug messages.

.. option:: ELF_FILE

   DPDK application binary or dynamic library.
   Any linked ``librte_*.so`` library (as reported by ``ldd``) will also be analyzed.
   Can be specified multiple times.

Environment Variables
---------------------

.. envvar:: LD_LIBRARY_PATH

   If specified, the linked ``librte_*.so`` libraries will be looked up here first.

Examples
--------

Get the complete info for a given driver:

.. code-block:: console

   $ dpdk-pmdinfo.py /usr/bin/dpdk-testpmd | \
       jq '.[] | select(.name == "net_ice_dcf")'
   {
     "name": "net_ice_dcf",
     "params": "cap=dcf",
     "kmod": "* igb_uio | vfio-pci",
     "pci_ids": [
       {
         "vendor": "8086",
         "device": "1889"
       }
     ]
   }

Get only the required kernel modules for a given driver:

.. code-block:: console

   $ dpdk-pmdinfo.py /usr/bin/dpdk-testpmd | \
       jq '.[] | select(.name == "net_cn10k").kmod'
   "vfio-pci"

Get only the required kernel modules for a given device:

.. code-block:: console

   $ dpdk-pmdinfo.py /usr/bin/dpdk-testpmd | \
       jq '.[] | select(.pci_ids[]? | .vendor == "15b3" and .device == "1013").kmod'
   "* ib_uverbs & mlx5_core & mlx5_ib"
