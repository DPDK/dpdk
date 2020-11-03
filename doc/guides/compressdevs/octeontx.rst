..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Cavium Networks.

OCTEON TX ZIP Compression Poll Mode Driver
==========================================

The OCTEON TX ZIP PMD (**librte_compress_octeontx**) provides poll mode
compression & decompression driver for ZIP HW offload device, found in
**Cavium OCTEON TX** SoC family.

More information can be found at `Cavium, Inc Official Website
<http://www.cavium.com/OCTEON-TX_ARM_Processors.html>`_.

Features
--------

OCTEON TX ZIP PMD has support for:

Compression/Decompression algorithm:

* DEFLATE

Huffman code type:

* FIXED
* DYNAMIC

Window size support:

* 2 to 2^14

Limitations
-----------

* Chained mbufs are not supported.

Supported OCTEON TX SoCs
------------------------

- CN83xx

Steps To Setup Platform
-----------------------

   OCTEON TX SDK includes kernel image which provides OCTEON TX ZIP PF
   driver to manage configuration of ZIPVF device
   Required version of SDK is "OCTEONTX-SDK-6.2.0-build35" or above.

   SDK can be install by using below command.
   #rpm -ivh OCTEONTX-SDK-6.2.0-build35.x86_64.rpm --force --nodeps
   It will install OCTEONTX-SDK at following default location
   /usr/local/Cavium_Networks/OCTEONTX-SDK/

   For more information on building and booting linux kernel on OCTEON TX
   please refer /usr/local/Cavium_Networks/OCTEONTX-SDK/docs/OcteonTX-SDK-UG_6.2.0.pdf.

   SDK and related information can be obtained from: `Cavium support site <https://support.cavium.com/>`_.

Initialization
--------------

The OCTEON TX zip is exposed as pci device which consists of a set of
PCIe VF devices. On EAL initialization, ZIP PCIe VF devices will be
probed. To use the PMD in an application, user must:

* run dev_bind script to bind eight ZIP PCIe VFs to the ``vfio-pci`` driver:

   .. code-block:: console

      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.1
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.2
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.3
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.4
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.5
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.6
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:00.7
      ./usertools/dpdk-devbind.py -b vfio-pci 0001:04:01.0

* The unit test cases can be tested as below:

   .. code-block:: console

      reserve enough huge pages
      cd to <build_dir>
      meson test compressdev_autotest
