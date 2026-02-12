..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _compiling_sample_apps:

Compiling and Running Sample Applications
=========================================

The chapter describes how to compile and run applications in a DPDK
environment. It also provides a pointer to where sample applications are stored.

Compiling a Sample Application
------------------------------

The DPDK example applications make use of the pkg-config file installed on
the system when DPDK is installed, and so can be built using GNU make.

.. note::

   BSD make cannot be used to compile the DPDK example applications. GNU
   make can be installed using `pkg install gmake` if not already installed
   on the FreeBSD system.

The following shows how to compile the helloworld example app, following
the installation of DPDK using `meson install` as described previously::

        $ export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

        $ cd examples/helloworld/

        $ gmake
        cc -O3 -I/usr/local/include -include rte_config.h -march=native
        -D__BSD_VISIBLE  main.c -o build/helloworld-shared
        -L/usr/local/lib -lrte_telemetry -lrte_bpf
        -lrte_pipeline -lrte_table -lrte_port -lrte_fib -lrte_ipsec
        -lrte_stack -lrte_security -lrte_sched -lrte_reorder -lrte_rib
        -lrte_rcu -lrte_rawdev -lrte_pdump -lrte_member -lrte_lpm
        -lrte_latencystats -lrte_jobstats -lrte_ip_frag -lrte_gso -lrte_gro
        -lrte_eventdev -lrte_efd -lrte_distributor -lrte_cryptodev
        -lrte_compressdev -lrte_cfgfile -lrte_bitratestats -lrte_bbdev
        -lrte_acl -lrte_timer -lrte_hash -lrte_metrics -lrte_cmdline
        -lrte_pci -lrte_ethdev -lrte_meter -lrte_net -lrte_mbuf
        -lrte_mempool -lrte_ring -lrte_eal -lrte_kvargs
        ln -sf helloworld-shared build/helloworld


.. _running_sample_app:

Running a Sample Application
----------------------------

#.  The ``contigmem`` and ``nic_uio`` modules must be set up prior to running an application.

#.  Any ports to be used by the application must be already bound to the ``nic_uio`` module,
    as described in section :ref:`binding_network_ports`, prior to running the application.
    The application is linked with the DPDK target environment's Environment
    Abstraction Layer (EAL) library, which provides some options that are generic
    to every DPDK application.

You can refer to :doc:`freebsd_eal_parameters` for the list of EAL options.

.. _running_non_root:

Running DPDK Applications Without Root Privileges
-------------------------------------------------

Although applications using the DPDK use network ports and other hardware
resources directly, with a number of small permission adjustments, it is possible
to run these applications as a user other than "root".  To do so, the ownership,
or permissions, on the following file system objects should be adjusted to ensure
that the user account being used to run the DPDK application has access
to them:

*   The userspace-io device files in ``/dev``, for example, ``/dev/uio0``, ``/dev/uio1``, and so on

*   The userspace contiguous memory device: ``/dev/contigmem``

.. note::

    Please refer to the DPDK Release Notes for supported applications.
