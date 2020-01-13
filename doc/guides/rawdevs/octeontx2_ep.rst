..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.

Marvell OCTEON TX2 End Point Rawdev Driver
==========================================

OCTEON TX2 has an internal SDP unit which provides End Point mode of operation
by exposing its IOQs to Host, IOQs are used for packet I/O between Host and
OCTEON TX2. Each OCTEON TX2 SDP PF supports a max of 128 VFs and Each VF is
associated with a set of IOQ pairs.

Features
--------

This OCTEON TX2 End Point mode PMD supports

#. Packet Input - Host to OCTEON TX2 with direct data instruction mode.

#. Packet Output - OCTEON TX2 to Host with info pointer mode.

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV`` (default ``y``)

  Toggle compilation of the ``lrte_pmd_octeontx2_ep`` driver.

Initialization
--------------

The number of SDP VFs enabled, can be controlled by setting sysfs
entry `sriov_numvfs` for the corresponding PF driver.

.. code-block:: console

 echo <num_vfs> > /sys/bus/pci/drivers/octeontx2-ep/0000\:04\:00.0/sriov_numvfs

Once the required VFs are enabled, to be accessible from DPDK, VFs need to be
bound to vfio-pci driver.
